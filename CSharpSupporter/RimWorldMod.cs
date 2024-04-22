using System.Reflection;
using System.Runtime.InteropServices;

namespace CSharpSupporter
{
	public static class RimWorldMod
	{
		public static List<string> AllTypesFromDef(string modAsmPath)
		{
			List<string> asmInNeed =
			[
				.. Directory.GetFiles(RuntimeEnvironment.GetRuntimeDirectory(), "*.dll"),
				RimWolrdVanilla.ASSEMBLY_ABS_PATH,
				"D:\\SteamLibrary\\steamapps\\common\\RimWorld\\Mods\\rjw\\1.4\\Assemblies\\RJW.dll",
				modAsmPath,
			];

			var resolver = new PathAssemblyResolver(asmInNeed);
			using var mlc = new MetadataLoadContext(resolver);

			var rimWorldAsm = mlc.LoadFromAssemblyPath(RimWolrdVanilla.ASSEMBLY_ABS_PATH);
			var modAssembly = mlc.LoadFromAssemblyPath(modAsmPath);

			var Def_t = rimWorldAsm.GetType("Verse.HediffComp");
			var modDefTypes =
								from ty in modAssembly.GetTypes()
								where ty.IsAssignableTo(Def_t)
								select ty;

			return modDefTypes.Select(t => t.Name).ToList();
		}
	}
}
