using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.InteropServices;

namespace CSharpSupporter
{
	public class ClassInfo
	{
		[SetsRequiredMembers]
		public ClassInfo(Type ty)
		{
			Namespace = ty.Namespace ?? "";
			Name = ty.Name;
			Base = ty.BaseType?.FullName ?? ty.BaseType?.Name ?? "";

			if (Base == typeof(System.Object).FullName)
				Base = ""; // This doesn't count!!
		}

		public required string Namespace;
		public required string Name;
		public required string Base;
		public List<string> MustTranslates = [];
		public List<string> ArraysMustTranslate = [];
		public List<(string, string)> ObjectArrays = [];
		public List<(string, string)> Objects = [];
	}

	public static class RimWolrdVanilla
	{
		public static string LastReadVersion = "Unset";
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
			var tEditable = rimWorldAsm.GetType("Verse.Editable");
			LastReadVersion = rimWorldAsm.GetName().Version?.ToString() ?? "Unknown";

			SortedDictionary<string, ClassInfo> classes = [];
			classes[tEditable!.FullName!] = new ClassInfo(tEditable);

			var ArrayOfObjects = new List<FieldInfo>();
			var PotentiallyTranslatableObject = new List<FieldInfo>();

			foreach (var ty in rimWorldAsm.GetTypes().Where(t => !t.IsGenericType))
			{
				try
				{
					var info = new ClassInfo(ty);

					// Inherited fields will be included.
					foreach (var field in ty.GetFields(BindingFlags.FlattenHierarchy | BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
					{
						try
						{
							if (field.CustomAttributes.Any(att => att.AttributeType.FullName?.Contains("MustTranslate") ?? false))
							{
								if (field.FieldType.IsGenericType || field.FieldType.IsArray)
									info.ArraysMustTranslate.Add(field.Name);
								else
									info.MustTranslates.Add(field.Name);
							}
							else if (field.FieldType.IsGenericType
								&& field.FieldType.GetGenericTypeDefinition().Name == "List`1"
								&& field.ReflectedType != null)
							{
								ArrayOfObjects.Add(field);
							}
							else if (rimWorldAsm.GetType(field.FieldType.FullName ?? field.FieldType.Name) != null)
							{
								PotentiallyTranslatableObject.Add(field);
							}
						}
						catch { }
					}

					if (info.MustTranslates.Count != 0 || info.ArraysMustTranslate.Count != 0)
						classes[ty.FullName ?? ty.Name] = info;
				}
				catch { }
			}

			foreach (var field in ArrayOfObjects
				.Where(f => classes.ContainsKey(f.FieldType.GenericTypeArguments.First().FullName ?? f.FieldType.GenericTypeArguments.First().Name))
				.Where(f => classes.ContainsKey(f.ReflectedType!.FullName ?? f.ReflectedType!.Name)))
			{
				classes[field.ReflectedType!.FullName ?? field.ReflectedType!.Name].ObjectArrays
					.Add((field.Name, field.FieldType.GenericTypeArguments.First().FullName ?? field.FieldType.GenericTypeArguments.First().Name));
			}

			foreach (var field in PotentiallyTranslatableObject
				.Where(f => classes.ContainsKey(f.FieldType.FullName ?? f.FieldType.Name))
				.Where(f => classes.ContainsKey(f.ReflectedType!.FullName ?? f.ReflectedType!.Name)))
			{
				classes[field.ReflectedType!.FullName ?? field.ReflectedType!.Name].Objects
					.Add((field.Name, field.FieldType.FullName ?? field.FieldType.Name));
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
