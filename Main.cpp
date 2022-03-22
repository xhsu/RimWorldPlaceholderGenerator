// RimWorldPlaceholderGenerator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "tinyxml2/tinyxml2.h"

import Mod;

int main(int argc, char** argv)
{
	tinyxml2::XMLDocument ret;
	ret.SetBOM(true);
	ret.InsertFirstChild(ret.NewDeclaration());

	std::ios_base::sync_with_stdio(false);
	//GetTranslatableEntriesOfMod("Test/1.3");
	GetDummyLocalizationOfFile("Test\\1.3\\Defs\\InteractionDef\\Interactions_Dialogue.xml", &ret);
	//auto rg = GetTranslatableEntriesOfFile("Test\\1.3\\Defs\\InteractionDef\\Interactions_Dialogue.xml");

	ret.SaveFile("ret.xml");
	return EXIT_SUCCESS;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
