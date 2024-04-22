using System.Reflection;
using System.Runtime.InteropServices;

namespace CSharpSupporter
{
	public class ClassInfo
	{
		public required string Namespace;
		public required string Name;
		public required string Base;
		public List<string> MustTranslates = [];
		public List<string> MustTranslateAsArray = [];
	}

	public static class RimWolrdVanilla
	{
		public const string ASSEMBLY_ABS_PATH = "D:\\SteamLibrary\\steamapps\\common\\RimWorld\\RimWorldWin64_Data\\Managed\\Assembly-CSharp.dll";

		public static IEnumerable<ClassInfo> FindWithoutNamespaces(this SortedDictionary<string, ClassInfo> self, string className)
		{
			return self
				.Where(kv => kv.Key.EndsWith(className))
				.Select(kv => kv.Value);
		}

		public static SortedDictionary<string, ClassInfo> GetClasses()
		{
			List<string> asmInNeed =
			[
				.. Directory.GetFiles(RuntimeEnvironment.GetRuntimeDirectory(), "*.dll"),
				ASSEMBLY_ABS_PATH,
			];

			var resolver = new PathAssemblyResolver(asmInNeed);
			using var mlc = new MetadataLoadContext(resolver);

			var rimWorldAsm = mlc.LoadFromAssemblyPath(ASSEMBLY_ABS_PATH);
			var tVerseDef = rimWorldAsm.GetType("Verse.Def");

			var defTypes =
				from ty in rimWorldAsm.GetTypes()
				where ty.Name.EndsWith("Def")
				where ty.IsAssignableTo(tVerseDef)
				select ty;

			// This one is special, just for the sake of 'Def' itself.
			defTypes = defTypes.Concat([rimWorldAsm.GetType("Verse.Editable")]);
			//var rit = rimWorldAsm.GetType("RimWorld.RitualOutcomeEffectDef");

			SortedDictionary<string, ClassInfo> classes = [];
			foreach (var ty in defTypes)
			{
				try
				{
					var info = new ClassInfo
					{
						Namespace = ty.Namespace ?? "",
						Name = ty.Name,
						Base = ty.BaseType?.FullName ?? ty.BaseType?.Name ?? "",
					};

					foreach (var field in ty.GetFields())   // Inherited fields will be included.
					{
						try
						{
							if (field.CustomAttributes.Any(att => att.AttributeType.FullName?.Contains("MustTranslate") ?? false))
							{
								if (field.FieldType.IsGenericType || field.FieldType.IsArray)
									info.MustTranslateAsArray.Add(field.Name);
								else
									info.MustTranslates.Add(field.Name);
							}
						}
						catch { }
					}

					if (info.Base == typeof(System.Object).FullName)
						info.Base = ""; // This doesn't count!!

					classes[ty.FullName ?? ty.Name] = info;
				}
				catch { }
			}

			return classes;
		}

		public static List<string> AllDefTypes()
		{
			List<string> asmInNeed =
			[
				.. Directory.GetFiles(RuntimeEnvironment.GetRuntimeDirectory(), "*.dll"),
				ASSEMBLY_ABS_PATH,
			];

			var resolver = new PathAssemblyResolver(asmInNeed);
			using var mlc = new MetadataLoadContext(resolver);

			var rimWorldAsm = mlc.LoadFromAssemblyPath(ASSEMBLY_ABS_PATH);
			var tVerseDef = rimWorldAsm.GetType("Verse.Def");
			/*
			List<string> EndsWithDef = [];
			foreach (var ty in rimWorldAsm.GetTypes())
			{
				try
				{
					if (ty.Name.EndsWith("Def"))
						EndsWithDef.Add(ty.FullName ?? ty.Name);
				}
				catch { }
			}

			List<string> DerivedFromDef = [];
			foreach (var ty in rimWorldAsm.GetTypes())
			{
				try
				{
					if (ty.IsAssignableTo(tVerseDef))
						DerivedFromDef.Add(ty.FullName ?? ty.Name);
				}
				catch { }
			}
			var EndsWithDef_unique = EndsWithDef.Except(DerivedFromDef).ToImmutableSortedSet();
			var DerivedFromDef_unique = DerivedFromDef.Except(EndsWithDef).ToImmutableSortedSet();
			*/

			var defTypes =
				from ty in rimWorldAsm.GetTypes()
				where ty.Name.EndsWith("Def")
				where ty.IsAssignableTo(tVerseDef)
				select ty.FullName;

			return defTypes.ToList();
		}

		public static List<string> AllTranslationFields()
		{
			List<string> asmInNeed =
			[
				.. Directory.GetFiles(RuntimeEnvironment.GetRuntimeDirectory(), "*.dll"),
				ASSEMBLY_ABS_PATH,
			];

			var resolver = new PathAssemblyResolver(asmInNeed);
			using var mlc = new MetadataLoadContext(resolver);

			var rimWorldAsm = mlc.LoadFromAssemblyPath(ASSEMBLY_ABS_PATH);

			List<string> ret = [];
			foreach (var ty in rimWorldAsm.GetTypes())
			{
				try
				{
					foreach (var field in ty.GetFields())
					{
						try
						{
							if (field.CustomAttributes.Any(att => att.AttributeType.FullName?.Contains("MustTranslate") ?? false))
								ret.Add($"{ty.FullName}.{field.Name}");
						}
						catch { }
					}
				}
				catch { }
			}

			return ret;
		}
	}
}
