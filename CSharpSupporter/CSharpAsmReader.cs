using System;
using System.IO;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

namespace CSharpSupporter
{
	// https://learn.microsoft.com/en-us/dotnet/api/system.reflection.pathassemblyresolver?view=dotnet-plat-ext-8.0
	public static class CSharpAsmReader
	{
		public static List<string> m_rgszAssemblyPaths = [];
		public static string m_szCurrent = "";

		public static void SetupAssemblies(string[]? additionalPaths = null)
		{
			// Get the array of runtime assemblies.
			string[] runtimeAssemblies = Directory.GetFiles(RuntimeEnvironment.GetRuntimeDirectory(), "*.dll");

			// Create the list of assembly paths consisting of runtime assemblies and the inspected assembly.
			m_rgszAssemblyPaths.Clear();
			m_rgszAssemblyPaths.AddRange(runtimeAssemblies);

			if (additionalPaths is not null)
				m_rgszAssemblyPaths.AddRange(additionalPaths);
		}

		public static void SetCurrent(string path)
		{
			m_rgszAssemblyPaths.Remove(m_szCurrent);
			m_rgszAssemblyPaths.Add(path);
			m_szCurrent = path;
		}

		public static Assembly AsmMeta()
		{
			var resolver = new PathAssemblyResolver(m_rgszAssemblyPaths);
			using var mlc = new MetadataLoadContext(resolver);

			return mlc.LoadFromAssemblyPath(m_szCurrent);
		}

		public static Type[] ReadTypes()
		{
			// Create PathAssemblyResolver that can resolve assemblies using the created list.
			var resolver = new PathAssemblyResolver(m_rgszAssemblyPaths);
			using var mlc = new MetadataLoadContext(resolver);

			// Load assembly into MetadataLoadContext.
			Assembly assembly = mlc.LoadFromAssemblyPath(m_szCurrent);

			return assembly.GetTypes();
		}
	}
}
