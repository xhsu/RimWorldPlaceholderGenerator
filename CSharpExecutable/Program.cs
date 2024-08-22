using Microsoft.VisualBasic.FileIO;
using System.Reflection;
using CSharpSupporter;
using System.Linq;

namespace CSharpExecutable
{
	static class Program
	{
		// Fuck C#, why the alignment has to be constant??

		const int LongestFullName = -63;
		const int LongestNamespace = -15;
		const int LongestName = -54;
		const int LongestBase = -57;

		public static void WriteArray(this StreamWriter file, List<string> arr)
		{
			if (arr.Count > 0)
			{
				var Text = string.Join("\", \"", arr);
				file.Write($"{{ \"{Text}\" }}, ");
			}
			else
			{
				file.Write("{}, ");
			}
		}

		public static void WritePairs(this StreamWriter file, List<(string, string)> arr)
		{
			if (arr.Count > 0)
			{
				var Text = string.Join(", ", arr.Select(p => $"{{ \"{p.Item1}\", \"{p.Item2}\" }}"));
				file.Write($"{{ {Text} }}, ");
			}
			else
			{
				file.Write("{}, ");
			}
		}

		public static void Main(string[] args)
		{
			var VanillaClassInfo = RimWolrdVanilla.GetClasses();
			//var LongestFullName_dyn = VanillaClassInfo.Keys.Max(s => s.Length) + 2;
			//var LongestNamespace_dyn = VanillaClassInfo.Values.Max(info => info.Namespace.Length) + 2;
			//var LongestName_dyn = VanillaClassInfo.Values.Max(info => info.Name.Length) + 2;
			//var LongestBase_dyn = VanillaClassInfo.Values.Max(info => info.Base.Length) + 2;

			//Console.WriteLine($"{LongestFullName_dyn}, {LongestNamespace_dyn}, {LongestName_dyn}, {LongestBase_dyn}");

			using var hpp = new StreamWriter("../../../../RimWorldClasses.hpp");

			hpp.WriteLine($"inline constexpr char RIMWORLD_ASSEMBLY_VERSION[] = \"{RimWolrdVanilla.LastReadVersion}\";");
			hpp.WriteLine();
			hpp.WriteLine("inline classinfo_dict_t const gRimWorldClasses");
			hpp.WriteLine("{");

			foreach (var (Name, Info) in VanillaClassInfo)
			{
				hpp.Write($"\t{{ {$"\"{Name}\"",LongestFullName}, {{ {$"\"{Info.Namespace}\"",LongestNamespace}, {$"\"{Info.Name}\"",LongestName}, {$"\"{Info.Base}\"",LongestBase}, ");
				hpp.WriteArray(Info.MustTranslates);
				hpp.WriteArray(Info.ArraysMustTranslate);
				hpp.WritePairs(Info.ObjectArrays);
				hpp.WritePairs(Info.Objects);
				hpp.WriteLine("}, },");
			}

			foreach (var (Name, Info) in VanillaClassInfo)
			{
				if (Info.Objects.Count > 0)
				{
					Console.WriteLine($"'{Name}' has following translatable sub-entry:");

					foreach (var (FieldName, TypeName) in Info.Objects)
						Console.WriteLine($"\t{FieldName}\t -> \t{TypeName}");
				}
			}

			hpp.WriteLine("};");
			hpp.WriteLine();

			Console.WriteLine("File 'RimWorldClasses.hpp' had been outputed.");
			Console.WriteLine($"{VanillaClassInfo.Count} classes were exported.");
		}
	}
}
