/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 31.03.2025

  Copyright (C) 2025, Johannes Natter

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef APP_HAS_TCLAP
#define APP_HAS_TCLAP 0
#endif

#if defined(__unix__)
#include <signal.h>
#endif
#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#endif
#include <iostream>
#include <chrono>
#include <thread>
#if APP_HAS_TCLAP
#include <tclap/CmdLine.h>
#endif

#if APP_HAS_TCLAP
#include "TclapOutput.h"
#endif
#include "GwSupervising.h"
#include "LibDspc.h"

#include "env.h"

using namespace std;
using namespace chrono;
#if APP_HAS_TCLAP
using namespace TCLAP;
#endif

#define dCodeUartDefault		"aaaaa"
#if defined(__unix__)
#define dDeviceUartDefault	"/dev/ttyACM0"
#elif defined(_WIN32)
#define dDeviceUartDefault	"COM1"
#else
#define dDeviceUartDefault	"uart-dev-undef"
#endif

const int cRateRefreshDefaultMs = 500;
const int cRateRefreshMinMs = 10;
const int cRateRefreshMaxMs = 20000;
#define dStartPortsOrbDefault "2000"
#define dStartPortsTargetDefault "3000"
const int cPortMax = 64000;

Environment env;
GwSupervising *pApp = NULL;

#if APP_HAS_TCLAP
class AppHelpOutput : public TclapOutput {};
#endif

/*
Literature
- http://man7.org/linux/man-pages/man7/signal.7.html
  - for enums: kill -l
  - sys/signal.h
  SIGHUP  1     hangup
  SIGINT  2     interrupt
  SIGQUIT 3     quit
  SIGILL  4     illegal instruction (not reset when caught)
  SIGTRAP 5     trace trap (not reset when caught)
  SIGABRT 6     abort()
  SIGPOLL 7     pollable event ([XSR] generated, not supported)
  SIGFPE  8     floating point exception
  SIGKILL 9     kill (cannot be caught or ignored)
- https://www.usna.edu/Users/cs/aviv/classes/ic221/s16/lec/19/lec.html
- http://www.alexonlinux.com/signal-handling-in-linux
*/
void applicationCloseRequest(int signum)
{
	(void)signum;
	cout << endl;
	pApp->unusedSet();
}

#if defined(_WIN32)
/*
Literature
- https://learn.microsoft.com/en-us/windows/console/setconsolectrlhandler
*/
BOOL WINAPI signalWinReceived(DWORD signal)
{
	if (signal != CTRL_C_EVENT)
		return FALSE;

	applicationCloseRequest(0);

	return TRUE;
}
#endif

#if !APP_HAS_TCLAP
void helpPrint()
{
	cout << endl << dPackageName << endl;
	cout << "Version: " << dVersion << endl;

	cout << endl;
	cout << "Usage: " << dAppName << " [code] [device]" << endl;
	cout << endl;
}
#endif

int main(int argc, char *argv[])
{
	env.haveTclap = 1;
	env.verbosity = 0;
#if defined(__unix__)
	env.coreDump = false;
#endif
	env.ctrlManual = 0;
	env.codeUart = dCodeUartDefault;
	env.deviceUart = dDeviceUartDefault;
	env.rateRefreshMs = cRateRefreshDefaultMs;

	env.startPortsOrb = atoi(dStartPortsOrbDefault);
	env.startPortsTarget = atoi(dStartPortsTargetDefault);

#if APP_HAS_TCLAP
	int res;

	CmdLine cmd("Command description message", ' ', appVersion());

	AppHelpOutput aho;
#if 1
	aho.package = dPackageName;
	aho.versionApp = dVersion;
	aho.nameApp = dAppName;
	aho.copyright = " (C) 2025 DSP-Crowd Electronics GmbH";
#endif
	cmd.setOutput(&aho);

	ValueArg<int> argVerbosity("v", "verbosity", "Verbosity: high => more output", false, 0, "uint8");
	cmd.add(argVerbosity);
#if defined(__unix__)
	SwitchArg argCoreDump("", "core-dump", "Enable core dumps", false);
	cmd.add(argCoreDump);
#endif
	SwitchArg argCtrlManual("", "ctrl-manual", "Use manual control (automatic control disabled)", false);
	cmd.add(argCtrlManual);
	ValueArg<string> argCodeUart("c", "code", "Code used for UART initialization. Default: " dCodeUartDefault,
								false, env.codeUart, "string");
	cmd.add(argCodeUart);
	ValueArg<string> argDevUart("d", "device", "Device used for UART communication. Default: " dDeviceUartDefault,
								false, env.deviceUart, "string");
	cmd.add(argDevUart);
	ValueArg<uint32_t> argRateRefreshMs("", "refresh-rate", "Refresh rate of process tree in [ms]",
								false, env.rateRefreshMs, "uint16");
	cmd.add(argRateRefreshMs);

	ValueArg<uint16_t> argStartPortOrb("", "start-ports-orb", "Start of 3-port interface for CodeOrb. Default: " dStartPortsOrbDefault,
								false, env.startPortsOrb, "uint16");
	cmd.add(argStartPortOrb);
	ValueArg<uint16_t> argStartPortTarget("", "start-ports-target", "Start of 3-port interface for the target. Default: " dStartPortsTargetDefault,
								false, env.startPortsTarget, "uint16");
	cmd.add(argStartPortTarget);

	cmd.parse(argc, argv);

	res = argVerbosity.getValue();
	if (res > 0 && res < 6)
		env.verbosity = res;

	env.ctrlManual = argCtrlManual.getValue() ? 1 : 0;
#if defined(__unix__)
	env.coreDump = argCoreDump.getValue();
#endif
	env.codeUart = argCodeUart.getValue();
	env.deviceUart = argDevUart.getValue();

	uint32_t ures = argRateRefreshMs.getValue();
	if (ures > cRateRefreshMinMs &&
			ures <= cRateRefreshMaxMs)
		env.rateRefreshMs = ures;

	res = argStartPortOrb.getValue();
	if (res > 0 && res <= cPortMax)
		env.startPortsOrb = res;

	res = argStartPortTarget.getValue();
	if (res > 0 && res <= cPortMax)
		env.startPortsTarget = res;
#else
	env.haveTclap = 0;
	env.verbosity = 2;

	if (argc >= 2)
		env.codeUart = string(argv[1]);
	if (argc >= 3)
		env.deviceUart = string(argv[2]);

	if (env.codeUart == "--help" ||
			env.codeUart == "-h")
	{
		helpPrint();
		return 0;
	}
#endif
	levelLogSet(env.verbosity);

#if defined(_WIN32)
	// https://learn.microsoft.com/en-us/windows/console/setconsolectrlhandler
	BOOL okWin;

	okWin = SetConsoleCtrlHandler(signalWinReceived, TRUE);
	if (!okWin)
	{
		errLog(-1, "could not set ctrl handler");
		return 1;
	}
#else
	// https://www.gnu.org/software/libc/manual/html_node/Termination-Signals.html
	signal(SIGINT, applicationCloseRequest);
	signal(SIGTERM, applicationCloseRequest);
#endif
	pApp = GwSupervising::create();
	if (!pApp)
	{
		errLog(-1, "could not create process");
		return 1;
	}

	pApp->procTreeDisplaySet(true);

	while (1)
	{
		for (int i = 0; i < 12; ++i)
			pApp->treeTick();

		if (!pApp->progress())
			break;

		this_thread::sleep_for(milliseconds(15));
	}

	Success success = pApp->success();
	Processing::destroy(pApp);

	Processing::applicationClose();

	filesStdClose();

	return !(success == Positive);
}

