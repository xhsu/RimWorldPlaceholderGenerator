using Microsoft.VisualBasic.FileIO;
using System.Reflection;
using CSharpSupporter;
using System.Linq;

namespace CSharpExecutable
{
	static class Program
	{
		// Fuck C#, why the alignment has to be constant??

		const int LongestFullName = -44;
		const int LongestNamespace = -14;
		const int LongestName = -35;
		const int LongestBase = -29;

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

		public static void Main(string[] args)
		{
			var VanillaClassInfo = RimWolrdVanilla.GetClasses();
			//var LongestFullName = VanillaClassInfo.Keys.Max(s => s.Length) + 3;
			//var LongestNamespace = VanillaClassInfo.Values.Max(info => info.Namespace.Length) + 3;
			//var LongestName = VanillaClassInfo.Values.Max(info => info.Name.Length) + 3;
			//var LongestBase = VanillaClassInfo.Values.Max(info => info.Base.Length) + 3;

			using var hpp = new StreamWriter("../../../../RimWorldClasses.hpp");

			hpp.WriteLine("inline classinfo_dict_t const gRimWorldClasses");
			hpp.WriteLine("{");

			foreach (var (Name, Info) in VanillaClassInfo)
			{
				hpp.Write($"\t{{ {$"\"{Name}\"",LongestFullName}, {{ {$"\"{Info.Namespace}\"",LongestNamespace}, {$"\"{Info.Name}\"",LongestName}, {$"\"{Info.Base}\"",LongestBase}, ");
				hpp.WriteArray(Info.MustTranslates);
				hpp.WriteArray(Info.MustTranslateAsArray);
				hpp.WriteLine("}, },");
			}

			hpp.WriteLine("};");
			hpp.WriteLine();

			Console.WriteLine("File RimWorldClasses.hpp had been outputed.");
			Console.WriteLine($"{VanillaClassInfo.Count} classes were exported.");
		}
	}
}
