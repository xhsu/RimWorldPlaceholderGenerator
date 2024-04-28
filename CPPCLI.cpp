//#using <CSharpSupporter.dll>
#using <System.Collections.dll>
#using <System.Console.dll>
#using <System.Core.dll>
#using <System.Linq.Queryable.dll>
#using <System.Linq.dll>
#using <System.Runtime.dll>
#using <System.dll>
#using <mscorlib.dll>

#include <filesystem>
#include <functional>
#include <map>
#include <ranges>
#include <span>
#include <string_view>
#include <string>
#include <vector>

#include <fmt/color.h>
#include <fmt/ranges.h>

#include "CPPCLI.hpp"
#include "Style.hpp"
extern classinfo_dict_t const gRimWorldClasses;


//using namespace CSharpSupporter;
using namespace System::Collections::Generic;
using namespace System::IO;
using namespace System::Linq;
using namespace System::Reflection;
using namespace System::Runtime::InteropServices;
using namespace System::Text;
using namespace System;

namespace fs = std::filesystem;


inline constexpr wchar_t ENGINE_REL_PATH[] = L"../../../RimWorldWin64_Data/Managed/";
inline constexpr wchar_t WORKSHOP_REL_PATH[] = L"../../../../../workshop/content/294100/";
inline constexpr wchar_t HARMONY[] = L"2009463077/1.4/Assemblies/0Harmony.dll";
inline constexpr wchar_t HUGSLIB[] = L"818773962/v1.4/Assemblies/HugsLib.dll";

ref class cliglb
{
public:
	static Dictionary<String^, Assembly^>^ LoadedAssembly = gcnew Dictionary<String ^, Assembly ^>;

public:
	static String^ ModPath = nullptr;
	static String^ ModsPath = nullptr;
	static String^ EnginePath = nullptr;
	static String^ WorkshopPath = nullptr;

	static void PathResolution(const char* mod)
	{
		ModPath = gcnew String(mod);

		// Is loading mod from RimWorld/mods?
		ModsPath = Path::GetFullPath(Path::Combine(ModPath, "../../"));
		EnginePath = Path::GetFullPath(Path::Combine(ModPath, gcnew String(ENGINE_REL_PATH)));
		WorkshopPath = Path::GetFullPath(Path::Combine(ModPath, gcnew String(WORKSHOP_REL_PATH)));

		if (Directory::Exists(ModsPath) && Directory::Exists(EnginePath) && Directory::Exists(WorkshopPath))
		{
			Console::WriteLine("Path resolution: Mod was placed in RimWorld/Mods/");
			return;
		}

		// In Workshop?
		ModsPath = Path::GetFullPath(Path::Combine(ModPath, "../../../../../common/RimWorld/Mods/"));
		EnginePath = Path::GetFullPath(Path::Combine(ModPath, "../../../../../common/RimWorld/RimWorldWin64_Data/Managed/"));
		WorkshopPath = Path::GetFullPath(Path::Combine(ModPath, "../../../"));

		if (Directory::Exists(ModsPath) && Directory::Exists(EnginePath) && Directory::Exists(WorkshopPath))
		{
			Console::WriteLine("Path resolution: Mod was placed in Steam/Workshop/Content/294100/");
			return;
		}

#ifdef _DEBUG
		ModsPath = Path::GetFullPath("D:\\SteamLibrary\\steamapps\\common\\RimWorld\\Mods");
		EnginePath = Path::GetFullPath("D:\\SteamLibrary\\steamapps\\common\\RimWorld\\RimWorldWin64_Data\\Managed\\");
		WorkshopPath = Path::GetFullPath("D:\\SteamLibrary\\steamapps\\workshop\\content\\294100\\");

		if (Directory::Exists(ModsPath) && Directory::Exists(EnginePath) && Directory::Exists(WorkshopPath))
		{
			Console::WriteLine("Path resolution: DEBUG_MODE - ABSOLUTE PATH ASSIGNED");
			return;
		}
#endif
	}
};

[[nodiscard]]
static std::string cli_to_stl(System::String^ s)
{
	array<unsigned char>^ bytes = Encoding::UTF8->GetBytes(s);
	pin_ptr<unsigned char> pinnedPtr = &bytes[0];

	return { (char*)pinnedPtr, (std::size_t)bytes->Length, };
}

static void ParseTypes(array<Type^>^ types, Type^ tTypeofDef, classinfo_dict_t* pret, bool(*pfnFilter)(Type^) = nullptr)
{
	/*
		var defTypes =
		from ty in rimWorldAsm.GetTypes()
		where ty.Name.EndsWith("Def")
		where ty.IsAssignableTo(tVerseDef)
		select ty;
	*/

	auto candidates = gcnew List<FieldInfo^>;

	for each (auto ty in types)
	{
		try
		{
			if (ty->IsGenericType)
				continue;

			if (pfnFilter && !pfnFilter(ty))
				continue;

			auto&& [iter, bNewEntry] = pret->try_emplace(
				cli_to_stl(ty->FullName != nullptr ? ty->FullName : ty->Name),
				class_info_t{
					.m_Namespace{ ty->Namespace == nullptr ? std::string{""} : cli_to_stl(ty->Namespace) },
					.m_Name{ cli_to_stl(ty->Name) },
					.m_Base{ cli_to_stl(ty->BaseType != nullptr ? ty->BaseType->FullName != nullptr ? ty->BaseType->FullName : ty->BaseType->Name : gcnew String("")) },
				}
			);

			if (!bNewEntry)
			{
				fmt::println("Duplicated name of '{}'", cli_to_stl(ty->FullName));
				continue;
			}

			auto& info = iter->second;

			for each (auto field in ty->GetFields())
			{
				try
				{
					bool bHandled = false;

					for each (auto attr in field->CustomAttributes)
					{
						if (attr->AttributeType->FullName != nullptr
							&& attr->AttributeType->FullName->Contains("MustTranslate"))
						{
							bHandled = true;

							if (field->FieldType->IsGenericType || field->FieldType->IsArray)
								info.m_ArraysMustTranslate.emplace(cli_to_stl(field->Name));
							else
								info.m_MustTranslates.emplace(cli_to_stl(field->Name));
						}
					}

					if (!bHandled
						&& field->FieldType->IsGenericType
						&& field->FieldType->GetGenericTypeDefinition()->Name == "List`1"
						&& field->ReflectedType != nullptr)
					{
						bHandled = true;
						candidates->Add(field);
					}
				}
				catch (...) { fmt::println("Members of type '{}' cannot be parse!", cli_to_stl(ty->FullName)); }
			}

			// Although in C# it make sense that everything derived from object.
			if (info.m_Base == "System.Object")
				info.m_Base.clear();

			// It's a class without any translation entry.
			if (info.m_MustTranslates.size() == 0 && info.m_ArraysMustTranslate.size() == 0)
				pret->erase(iter);
		}
		catch (...) { fmt::println("Type '{}' is inaccessible!", cli_to_stl(ty->FullName)); }
	}

	for each (auto field in candidates)
	{
		Type^ ElemType = field->FieldType->GenericTypeArguments[0];
		Type^ ReflType = field->ReflectedType;

		auto ElemName = cli_to_stl(ElemType->FullName != nullptr ? ElemType->FullName : ElemType->Name);
		auto ReflName = cli_to_stl(ReflType->FullName != nullptr ? ReflType->FullName : ReflType->Name);

		// Make sure both elem and refl are either in the return list or in base game.
		if ((!pret->contains(ElemName) && !gRimWorldClasses.contains(ElemName))
			|| (!pret->contains(ReflName) && !gRimWorldClasses.contains(ReflName)))
			continue;

		pret->at(ReflName).m_ObjectArrays.try_emplace(
			cli_to_stl(field->Name),
			std::move(ElemName)
		);
	}
}

static void GetModClasses(Assembly^ dll, Type^ tTypeofDef, classinfo_dict_t* pret)
{
	try
	{
		ParseTypes(dll->GetTypes(), tTypeofDef, pret);
	}
	// https://stackoverflow.com/questions/1091853/error-message-unable-to-load-one-or-more-of-the-requested-types-retrieve-the-l
	catch (ReflectionTypeLoadException^ ex)
	{
		fmt::print(Style::Error, "[::GetModClasses] Error encountered when parsing assembly '{}'", cli_to_stl(dll->GetName()->Name));
		fmt::print(Style::Info, "\n");

		auto sb = gcnew StringBuilder();

		for each(auto exSub in ex->LoaderExceptions)
		{
			sb->AppendLine(exSub->Message);
			auto exFileNotFound = dynamic_cast<FileNotFoundException^>(exSub);

			if (exFileNotFound != nullptr)
			{
				if (!String::IsNullOrEmpty(exFileNotFound->FusionLog))
				{
					sb->AppendLine("Fusion Log:");
					sb->AppendLine(exFileNotFound->FusionLog);
				}
			}

			sb->AppendLine();
		}

		Console::WriteLine(sb->ToString());
	}
}

static Assembly^ LoadAssembly(String^ dll, bool bShowLog)
{
	if (auto abs_path = Path::GetFullPath(dll);
		!cliglb::LoadedAssembly->ContainsKey(abs_path))
	{
		if (bShowLog)
		{
			fmt::print(
				Style::Skipping, "Loading Assembly: {0}\n",
				cli_to_stl(abs_path)
			);
		}

		// Regarding LoadFile() and LoadFrom()
		// https://stackoverflow.com/questions/1477843/difference-between-loadfile-and-loadfrom-with-net-assemblies

		auto asmb = Assembly::LoadFrom(abs_path);
		cliglb::LoadedAssembly[abs_path] = asmb;
		return asmb;
	}
	else
	{
		return cliglb::LoadedAssembly[abs_path];
	}
}

[[nodiscard]]
static List<Assembly^>^ LoadAllAssemblyFromDir(String^ dir_)
{
	auto ret = gcnew List<Assembly^>;
	auto dir = fs::path{ cli_to_stl(dir_) };

	if (!fs::exists(dir))
		return ret;

	int32_t LoadedCount = 0;
	Console::ForegroundColor = ConsoleColor::DarkGray;

	try
	{
		for (auto&& dll :
			fs::recursive_directory_iterator(dir)
			| std::views::transform(&fs::directory_entry::path)
			| std::views::filter([](fs::path const& pth) { return pth.has_extension() && pth.extension() == ".dll"; })
			)
		{
			++LoadedCount;
			ret->Add(LoadAssembly(gcnew String(dll.c_str()), false));
		}
	}
	catch (Exception^ ex)
	{
		Console::WriteLine("Error encounter in '::LoadAllAssemblyFromDir': {0}", ex->ToString());
	}

	fmt::print(
		Style::Skipping, "{0} assembl{2} loaded from '{1}'\n",
		LoadedCount, dir.u8string(), LoadedCount < 2 ? "y" : "ies"
	);

	return ret;
}

[[nodiscard]]
static Assembly^ LoadUnityEngine(String^ eng_dir)
{
	auto assemblies = LoadAllAssemblyFromDir(eng_dir);

	for each (auto asmb in assemblies)
	{
		if (asmb->GetName()->Name == "Assembly-CSharp")
			return asmb;
	}

	return nullptr;
}

void GetModClasses(const char* path_to_mod, classinfo_dict_t* pret)
{
	cliglb::PathResolution(path_to_mod);
	auto asm_dir = Path::GetFullPath(Path::Combine(cliglb::ModPath, L"Assemblies/"));

	if (!Directory::Exists(asm_dir))
		return;

	auto RimWorld = LoadUnityEngine(cliglb::EnginePath);
	auto tTypeofDef = RimWorld->GetType("Verse.Def");

	LoadAssembly(Path::Combine(cliglb::WorkshopPath, gcnew String(HUGSLIB)), true);
	LoadAssembly(Path::Combine(cliglb::WorkshopPath, gcnew String(HARMONY)), true);

	// classinfo_dict_t ret{};
	auto assemblies = LoadAllAssemblyFromDir(cliglb::ModPath);	// perhaps all modules rather than stuff in "Assemblies/"?
	for each (auto assembly in assemblies)
		GetModClasses(assembly, tTypeofDef, pret);

	fmt::print(
		Style::Positive,
		"{0} types loaded from mod.\n", pret->size()
	);
	fmt::println("");
}
