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

#include <fmt/core.h>
#include <fmt/ranges.h>

#include "CPPCLI.hpp"


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
	static String^ EnginePath = nullptr;
	static String^ WorkshopPath = nullptr;

	static void PathResolution(const char* mod)
	{
		ModPath = gcnew String(mod);

		// Is loading mod from RimWorld/mods?
		EnginePath = Path::GetFullPath(Path::Combine(ModPath, gcnew String(ENGINE_REL_PATH)));
		WorkshopPath = Path::GetFullPath(Path::Combine(ModPath, gcnew String(WORKSHOP_REL_PATH)));

		if (Directory::Exists(EnginePath) && Directory::Exists(WorkshopPath))
		{
			Console::WriteLine("Path resolution: Mod was placed in RimWorld/Mods/");
			return;
		}

		// In Workshop?
		EnginePath = Path::GetFullPath(Path::Combine(ModPath, "../../../../../common/RimWorld/RimWorldWin64_Data/Managed/"));
		WorkshopPath = Path::GetFullPath(Path::Combine(ModPath, "../../../"));

		if (Directory::Exists(EnginePath) && Directory::Exists(WorkshopPath))
		{
			Console::WriteLine("Path resolution: Mod was placed in Steam/Workshop/Content/294100/");
			return;
		}

#ifdef _DEBUG
		EnginePath = Path::GetFullPath("D:\\SteamLibrary\\steamapps\\common\\RimWorld\\RimWorldWin64_Data\\Managed\\");
		WorkshopPath = Path::GetFullPath("D:\\SteamLibrary\\steamapps\\workshop\\content\\294100\\");

		if (Directory::Exists(EnginePath) && Directory::Exists(WorkshopPath))
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

	for each (auto ty in types)
	{
		try
		{
			if (!tTypeofDef->IsAssignableFrom(ty))
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
					for each (auto attr in field->CustomAttributes)
					{
						if (attr->AttributeType->FullName != nullptr
							&& attr->AttributeType->FullName->Contains("MustTranslate"))
						{
							info.m_MustTranslates.emplace_back(cli_to_stl(field->Name));
						}
					}
				}
				catch (...) { fmt::println("Members of type '{}' cannot be parse!", cli_to_stl(ty->FullName)); }
			}

			if (info.m_Base == "System.Object")
				info.m_Base.clear();
		}
		catch (...) { fmt::println("Type '{}' is inaccessible!", cli_to_stl(ty->FullName)); }
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

[[nodiscard]]
classinfo_dict_t GetVanillaClassInfo(const wchar_t* rim_world_dll)
{
	auto assembly = Assembly::LoadFrom(gcnew String(rim_world_dll));
	auto types = assembly->GetTypes();

	auto tTypeofDef = assembly->GetType(gcnew String(CLASSNAME_VERSE_DEF));
	auto tTypeofEditable = assembly->GetType("Verse.Editable");	// This one is special, just for the sake of 'Def' itself.

	classinfo_dict_t ClassInfo
	{
		// The first item: Verse.Editable
		std::pair{
			cli_to_stl(tTypeofEditable->FullName),
			class_info_t{
				.m_Namespace{ cli_to_stl(tTypeofEditable->Namespace) },
				.m_Name{ cli_to_stl(tTypeofEditable->Name) },
				.m_Base{ "" },	// "System.Object", skipped
			},
		},
	};

	ParseTypes(types, tTypeofDef, &ClassInfo, [](Type^ ty) { return ty->Name->EndsWith("Def"); });
	return ClassInfo;
}

static Assembly^ LoadAssembly(String^ dll, bool bShowLog)
{
	if (auto abs_path = Path::GetFullPath(dll);
		!cliglb::LoadedAssembly->ContainsKey(abs_path))
	{
		if (bShowLog)
		{
			Console::ForegroundColor = ConsoleColor::DarkGray;
			Console::WriteLine("Loading Assembly: {0}", abs_path);
			Console::ForegroundColor = ConsoleColor::White;
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

	Console::WriteLine("{0} assembl{2} loaded from '{1}'", LoadedCount, dir_, LoadedCount < 2 ? "y" : "ies");
	Console::ForegroundColor = ConsoleColor::White;
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

[[nodiscard]]
class_info_t const* GetRootDefClassName(class_info_t const& info, std::span<classinfo_dict_t const*> dicts) noexcept
{
	if (info.m_Base.empty())
		return &info;

	if (info.m_Base == std::string_view{ CLASSNAME_VERSE_DEF })
		return &info;

	for (auto&& dict : dicts)
	{
		if (dict->contains(info.m_Base))
			return GetRootDefClassName(dict->at(info.m_Base), dicts);
	}

	return nullptr;
}

[[nodiscard]]
classinfo_dict_t GetModClasses(const char* path_to_mod)
{
	cliglb::PathResolution(path_to_mod);
	auto asm_dir = Path::GetFullPath(Path::Combine(cliglb::ModPath, L"Assemblies/"));

	if (!Directory::Exists(asm_dir))
		return {};

	auto RimWorld = LoadUnityEngine(cliglb::EnginePath);
	auto tTypeofDef = RimWorld->GetType("Verse.Def");

	LoadAssembly(Path::Combine(cliglb::WorkshopPath, gcnew String(HUGSLIB)), true);
	LoadAssembly(Path::Combine(cliglb::WorkshopPath, gcnew String(HARMONY)), true);

	classinfo_dict_t ret{};
	auto assemblies = LoadAllAssemblyFromDir(asm_dir);
	for each (auto assembly in assemblies)
		GetModClasses(assembly, tTypeofDef, &ret);

	Console::BackgroundColor = ConsoleColor::White;
	Console::ForegroundColor = ConsoleColor::DarkBlue;
	Console::WriteLine("{0} types loaded from mod.", ret.size());
	Console::BackgroundColor = ConsoleColor::Black;
	Console::ForegroundColor = ConsoleColor::White;
	Console::Write(L'\n');

	return ret;
}
