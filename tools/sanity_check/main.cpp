/*
 *
 *   program:  sanityCheck
 *   file:     main.c
 *   author:   Todd Naugle
 *   date:     11/17/2010
 *
 *   Desc:  Command line version of the sanity check functions found in jack
*/

#include <algorithm>
#include <stdio.h>
#include <string>
#include <vector>

#include "systemtest.h"


using namespace std;

typedef int (*testfuncPtr) ();
typedef int (*testfuncOpPtr) (string);

typedef struct
{
	string			switchText;			// ie -option
	string			swOptionText;       // option arguments for just this swtich.
	string			descriptionText;	// Help Text on what this does
	string			failureText;		// What to say when this test fails
	bool			hasOption;			// Set true if this switch has option paramters
	testfuncPtr		functionPtr;		// Function to call
	testfuncOpPtr	opFunctionPtr;		// Function with option string to call
	string			optionArg;			// Storage used to hold any options passed in by the user
} testRecord;

static vector<testRecord>	gTestSet;

static vector<string>		gValidSwitchList;
static vector<string>		gSwitchDescriptionList;

static vector<string>		gSwitchesReceived;

int
ExecuteAll()
{
	bool OK = true;

	OK &= system_user_can_rtprio();

	if (system_has_frequencyscaling()) {
		OK &= !system_uses_frequencyscaling();
	}

	OK &= !(system_memlock_amount() == 0);

	return OK;
}

int
HasGroup(string name)
{
	return system_has_group(name.c_str());
}

int
IsMemberOfGroup(string name)
{
	return system_user_in_group(name.c_str());
}

int
CheckFreqScaling()
{
	bool OK = true;

	if (system_has_frequencyscaling()) {
		OK &= !system_uses_frequencyscaling();
	}

	return OK;
}

int
CheckMemoryLocking()
{
	return !(system_memlock_amount() == 0);
}

int
PrintUsage()
{
	printf("\n");
	printf("  sanityCheck - A program to verify proper system settings for use with audio applications (Ardour/Jack/Mixbus).\n");
	printf("\n");
	printf("  Usage:  sanityCheck [OPTIONS]\n");
	printf("\n");
	printf("  Options are as follows:\n");
	printf("\n");
	printf("\n");

	vector<testRecord>::iterator		itr;

	for (itr = gTestSet.begin(); itr != gTestSet.end(); ++itr) {
		printf("%20s %s :\t%s\n", (*itr).switchText.c_str(), (*itr).swOptionText.c_str(), (*itr).descriptionText.c_str());
	}

	printf("\n");

	return true;
}

void
DefineSwitches()
{
	testRecord rec;

	// Global switches
	rec.switchText = "-a";
	rec.swOptionText = "";
	rec.descriptionText = "Checks for a working RT system. Same as -rt -freqscaling -memlock";
	rec.failureText = "";
	rec.hasOption = false;
	rec.functionPtr = &ExecuteAll;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	rec.switchText = "-h";
	rec.swOptionText = "";
	rec.descriptionText = "Print usage";
	rec.failureText = "";
	rec.hasOption = false;
	rec.functionPtr = &PrintUsage;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	// Switches for various tests that can be performed.
	rec.switchText = "-rt";
	rec.swOptionText = "";
	rec.descriptionText = "Verify that the user can run tasks with realtime priority";
	rec.failureText = "";
	rec.hasOption = false;
	rec.functionPtr = &system_user_can_rtprio;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	rec.switchText = "-hasrtlimits";
	rec.swOptionText = "";
	rec.descriptionText = "Verify the system has a limits.conf and the audio group can use realtime";
	rec.failureText = "";
	rec.hasOption = false;
	rec.functionPtr = &system_has_rtprio_limits_conf;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	rec.switchText = "-hasgroup";
	rec.swOptionText = "<groupname>";
	rec.descriptionText = "Verify that the system has a group named <groupname>";
	rec.failureText = "";
	rec.hasOption = true;
	rec.opFunctionPtr = &HasGroup;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	rec.switchText = "-hasaudiogroup";
	rec.swOptionText = "";
	rec.descriptionText = "Verify that the system has an audio group (audio or jackuser) defined";
	rec.failureText = "";
	rec.hasOption = false;
	rec.functionPtr = &system_has_audiogroup;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	rec.switchText = "-memberofgroup";
	rec.swOptionText = "<groupname>";
	rec.descriptionText = "Verify that the user is a member of the group named <groupname>";
	rec.failureText = "";
	rec.hasOption = true;
	rec.opFunctionPtr = &IsMemberOfGroup;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	rec.switchText = "-memberaudiogroup";
	rec.swOptionText = "";
	rec.descriptionText = "Verify that the user is a member of the audio group (audio or jackuser)";
	rec.failureText = "";
	rec.hasOption = false;
	rec.functionPtr = &system_user_in_audiogroup;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	rec.switchText = "-freqscaling";
	rec.swOptionText = "";
	rec.descriptionText = "Check to see if frequency scaling is being used by the CPU";
	rec.failureText = "";
	rec.hasOption = false;
	rec.functionPtr = &CheckFreqScaling;
	rec.optionArg = "";
	gTestSet.push_back(rec);

	rec.switchText = "-memlock";
	rec.swOptionText = "";
	rec.descriptionText = "Check to see if the user is able to lock memory";
	rec.failureText = "";
	rec.hasOption = false;
	rec.functionPtr = &CheckMemoryLocking;
	rec.optionArg = "";
	gTestSet.push_back(rec);

}

bool
ParseSwitches(
	int		argc,
	char	**argv)
{
	string	tmp;
	vector<testRecord>::iterator	itr;
	bool	OK = true;
	int	i;

	if (argc == 1) {
		gSwitchesReceived.push_back("-a");
	}
	else {
		for (i = 1; i < argc && OK == true; i++) {
			tmp = argv[i];

			for (itr = gTestSet.begin(); itr != gTestSet.end(); ++itr) {
				if (tmp == (*itr).switchText) {
					if ((*itr).hasOption == true) {
						if (++i < argc) {
							string	op = argv[i];
							if (op[0] == '-') {
								// reqiured option for this switch is missing
								--i;
								OK = false;
								break;
							}
							(*itr).optionArg = op;
							break;
						}
						else {
							// reqiured option for this switch is missing
							--i;
							OK = false;
							break;
						}
					}
					break;
				}
			}

			if (OK && itr != gTestSet.end()) {
				// Known option switch found
				gSwitchesReceived.push_back(tmp);
			}
			else {
				// Unknown option
				OK = false;
			}
		}
	}

	if (OK) {
		// All switches are at least valid, now check to make sure they are all valid to
		// be used together.

		if (gSwitchesReceived.size() > 1) {
			// make sure help is not mixed with other options
			vector<string>::iterator	swItr;
			tmp = "-h";

			swItr = find(gSwitchesReceived.begin(), gSwitchesReceived.end(), tmp);

			if (swItr != gSwitchesReceived.end()) {
				gSwitchesReceived.clear();
				gSwitchesReceived.push_back("-h");
			}

			// make sure -a is only used by itself
			tmp = "-a";
			swItr = find(gSwitchesReceived.begin(), gSwitchesReceived.end(), tmp);

			if (swItr != gSwitchesReceived.end()) {
				gSwitchesReceived.clear();
				gSwitchesReceived.push_back("-a");
			}
		}

		return true;
	}
	else {
		fprintf(stderr, "\n");
		fprintf(stderr, "ERROR - Invalid Option: %s\n", (const char *) argv[--i]);
		fprintf(stderr, "Check syntax\n");
		PrintUsage();
		return false;
	}
}

bool
Execute()
{
	bool OK = true;
	vector<string>::iterator	itr;
	vector<testRecord>::iterator	testItr;

	for (itr = gSwitchesReceived.begin(); itr != gSwitchesReceived.end(); ++itr) {
		for (testItr = gTestSet.begin(); testItr != gTestSet.end(); ++testItr) {
			if ((*itr) == (*testItr).switchText) {
				break;
			}
		}

		bool result;
		if ((*testItr).hasOption) {
			result = ((*testItr).opFunctionPtr((*testItr).optionArg) != 0);
		}
		else {
			result = ((*testItr).functionPtr() != 0);
		}

		if (result == 0) {
			// Check for a Failure message and print it if found.
			if (!(*testItr).failureText.empty()) {
				printf("\n%s\n", (*testItr).failureText.c_str());
			}
		}

		OK &= result;
	}

	return OK;
}

int
main(
	int		 argc,
	char 	**argv)
{
	int status = 0;

	DefineSwitches();

	if (ParseSwitches(argc, argv)) {
		if (Execute() == false) {
			printf("\nSanity Check Failed!\n\n");
			status = -1;
		}
		else {
			printf("\nSanity Check OK!\n\n");
		}
	}
	else {
		status = -1;
	}

	return status;
}
