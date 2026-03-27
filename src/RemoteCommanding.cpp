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

#include <sstream>

#include "RemoteCommanding.h"
#include "SingleWireScheduling.h"
#include "LibTime.h"

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StTransCreate) \
		gen(StTransSendReadyWait) \
		gen(StCmdAutoReceiveWait) \
		gen(StCmdAutoDoneWait) \
		gen(StFiltCreate) \
		gen(StFiltSendReadyWait) \
		gen(StWelcomeSend) \
		gen(StMain) \
		gen(StResponseRcvdWait) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 0
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

using namespace std;

// https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit
#define dColorGreen		"\033[38;5;46m"
#define dColorOrange	"\033[38;5;220m"
#define dColorGrey		"\033[38;5;240m"
#define dColorClear		"\033[0m"

#define dSizeHistoryMax 41

const string cWelcomeMsg = "\r\n" dPackageName "\r\n" \
			"Remote Terminal\r\n\r\n" \
			"type 'help' or just 'h' for a list of available commands\r\n\r\n";

const string cInternalCmdCls = "dbg";
const size_t cSizeColCmdMax = 22;
const uint32_t cTmoCmdAuto = 200;

list<EntryHelp> RemoteCommanding::cmds;

RemoteCommanding::RemoteCommanding(SOCKET fd)
	: Processing("RemoteCommanding")
	, mpTargetIsOnline(NULL)
	, mStartMs(0)
	, mModeAuto(false)
	, mFdSocket(fd)
	, mpTrans(NULL)
	, mpFilt(NULL)
	, mTxtPrompt()
	, mIdReq(0)
	, mTimestamps(1)
	// target online check
	, mTargetIsOnline(false)
	// command response measurement
	, mStartCmdMs(0)
	, mDelayResponseCmdMs(0)
	// command history
	, mCmdLast(U"")
	, mHistory()
	// auto completion
	, mLastKeyWasTab(false)
	, mCursorEditLow(0)
	, mStrEdit(U"")
{
	mBufOut[0] = 0;
	miEntryHist = mHistory.end();

	mState = StStart;
}

/* member functions */

Success RemoteCommanding::process()
{
	uint32_t curTimeMs = millis();
	uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
	PipeEntry<KeyUser> entKey;
	KeyUser key;
#if 0
	dStateTrace;
#endif
	switch (mState)
	{
	case StStart:

		if (mFdSocket == INVALID_SOCKET)
			return procErrLog(-1, "socket file descriptor not set");

		if (!mpTargetIsOnline)
			return procErrLog(-1, "target online pointer not set");

		if (mModeAuto)
		{
			mState = StTransCreate;
			break;
		}

		mState = StFiltCreate;

		break;
	case StTransCreate:

		mpTrans = TcpTransfering::create(mFdSocket);
		if (!mpTrans)
			return procErrLog(-1, "could not create process");

		mpTrans->procTreeDisplaySet(false);
		start(mpTrans);

		mState = StTransSendReadyWait;

		break;
	case StTransSendReadyWait:

		success = mpTrans->success();
		if (success != Pending)
			return success;

		if (!mpTrans->mSendReady)
			break;

		mStartMs = curTimeMs;
		mState = StCmdAutoReceiveWait;

		break;
	case StCmdAutoReceiveWait:

		if (diffMs > cTmoCmdAuto)
		{
			string str = "<timeout receiving command>\r\n";
			mpTrans->send(str.c_str(), str.size());
			return procErrLog(-1, "timeout receiving command");
		}

		success = autoCommandProcess();
		if (success == Pending)
			break;

		if (success != Positive)
			return procErrLog(-1, "could not process auto command");

		mStartMs = curTimeMs;
		mState = StCmdAutoDoneWait;

		break;
	case StCmdAutoDoneWait:

		if (diffMs > cTimeoutCommandResponseMs)
		{
			string str = "<command response timeout>\r\n";
			mpTrans->send(str.c_str(), str.size());
			return Positive;
		}

		success = responseReceive();
		if (success == Pending)
			break;

		//procWrnLog("auto mode done");

		return Positive;

		break;
	case StFiltCreate:

		mpFilt = TelnetFiltering::create(mFdSocket);
		if (!mpFilt)
			return procErrLog(-1, "could not create process");

		mpFilt->titleSet("CodeOrb");

		mpFilt->procTreeDisplaySet(false);
		start(mpFilt);

		mState = StFiltSendReadyWait;

		break;
	case StFiltSendReadyWait:

		success = mpFilt->success();
		if (success != Pending)
			return success;

		if (!mpFilt->mSendReady)
			break;

		mTxtPrompt.widthSet(25);
		mTxtPrompt.lenMaxSet(51);
		mTxtPrompt.cursorBoundSet(2);
		mTxtPrompt.focusSet(true);

		mTxtPrompt.frameEnabledSet(false);
		mTxtPrompt.paddingEnabledSet(false);

		mTargetIsOnline = !*mpTargetIsOnline;

		mState = StWelcomeSend;

		break;
	case StWelcomeSend:

		mpFilt->send(cWelcomeMsg.c_str(), cWelcomeMsg.size());
		promptSend();

		mState = StMain;

		break;
	case StMain:

		success = mpFilt->success();
		if (success != Pending)
			return success;

		if (stateOnlineChanged())
		{
			promptSend();
			break;
		}

		if (mpFilt->ppKeys.get(entKey) < 1)
			break;
		key = entKey.particle;

		//procWrnLog("Got key: %s", key.str().c_str());

		if (key == keyTab)
		{
			tabProcess();
			break;
		}

		mLastKeyWasTab = false;

		if (historyNavigate(key))
		{
			promptSend();
			break;
		}

		if (key == keyEnter)
		{
			success = commandSend();
			if (success == Positive)
				break;

			if (success != Pending)
				break;

			lineAck();

			mStartCmdMs = curTimeMs;

			mStartMs = curTimeMs;
			mState = StResponseRcvdWait;
			break;
		}

		if (key == keyUp || key == keyDown)
			break;

		if (mTxtPrompt.keyProcess(key))
		{
			promptSend();
			break;
		}

		break;
	case StResponseRcvdWait:

		if (diffMs > cTimeoutCommandResponseMs)
		{
			string msg;

			msg += dColorGrey;
			msg += "<command response timeout>";
			msg += dColorClear;
			msg += "\r\n";

			mpFilt->send(msg.c_str(), msg.size());
			promptSend();

			mState = StMain;
			break;
		}

		success = responseReceive();
		if (success == Pending)
			break;

		mState = StMain;

		break;
	default:
		break;
	}

	return Pending;
}

Success RemoteCommanding::autoCommandProcess()
{
	size_t lenReq;
	ssize_t lenDone;
	char *pBufIn = mBufOut;
	string str;
	bool ok;

	lenReq = sizeof(mBufOut) - 1;

	lenDone = mpTrans->read(pBufIn, lenReq);
	if (!lenDone)
		return Pending;

	if (lenDone < 0)
	{
		str = "<could not receive command>\r\n";
		mpTrans->send(str.c_str(), str.size());
		return procErrLog(-1, "could not receive command");
	}

	pBufIn[lenDone] = 0;

	// remove newline

	if (pBufIn[lenDone - 1] == '\n')
		pBufIn[--lenDone] = 0;

	if (pBufIn[lenDone - 1] == '\r')
		pBufIn[--lenDone] = 0;

	for (ssize_t i = 0; i < lenDone; ++i)
	{
		if (!SingleWireScheduling::isCtrl(pBufIn[i]))
			continue;

		str = "<command contains protocol control byte>\r\n";
		mpTrans->send(str.c_str(), str.size());
		return procErrLog(-1, "command contains protocol control byte");
	}
#if 0
	procInfLog("auto bytes received    %d", lenDone);
	procInfLog("auto command received  '%s'", pBufIn);

	for (ssize_t i = 0; i < lenDone; ++i)
		procInfLog("byte: %3u %02x '%c'", pBufIn[i], pBufIn[i], pBufIn[i]);
#endif
	str = string(pBufIn);

	ok = SingleWireScheduling::commandSend(str, mIdReq);
	if (!ok)
	{
		str = "<could not send command>\r\n";
		mpTrans->send(str.c_str(), str.size());
		return procErrLog(-1, "could not send command");
	}

	return Positive;
}

bool RemoteCommanding::stateOnlineChanged()
{
	if (*mpTargetIsOnline == mTargetIsOnline)
		return false;

	mTargetIsOnline = *mpTargetIsOnline;

	return true;
}

Success RemoteCommanding::commandSend()
{
	string str, msg;
	bool ok;

	u32string ustr = mTxtPrompt.ustrWork();

	if (ustr.size() && ustr.back() == U' ')
		ustr.pop_back();

	if (!ustr.size())
	{
		lineAck();
		promptSend();
		return Positive;
	}

	utfToStr(ustr, str);

	mCmdLast = ustr;

	if (str == "help" || str == "h")
	{
		mBufOut[0] = 0;
		cmdHelpPrint(NULL, mBufOut, mBufOut + sizeof(mBufOut));

		lfToCrLf(mBufOut, msg);

		if (msg.size())
			msg += "\r\n";

		lineAck();
		mpFilt->send(msg.c_str(), msg.size());
		promptSend();

		return Positive;
	}

	if (str == "timestampsToggle")
	{
		mTimestamps ^= 1;

		msg += dColorGrey;
		msg += "<timestamps ";
		msg += mTimestamps ? "en" : "dis";
		msg += "abled>";
		msg += dColorClear;

		msg += "\r\n";

		lineAck();
		mpFilt->send(msg.c_str(), msg.size());
		promptSend();

		return Positive;
	}

	if (str == "monitoringToggle")
	{
		SingleWireScheduling::monitoring ^= 1;

		msg += dColorGrey;
		msg += "<monitoring ";
		msg += SingleWireScheduling::monitoring ? "en" : "dis";
		msg += "abled>";
		msg += dColorClear;

		msg += "\r\n";

		lineAck();
		mpFilt->send(msg.c_str(), msg.size());
		promptSend();

		return Positive;
	}

	//procWrnLog("sending command: %s", str.c_str());

	ok = SingleWireScheduling::commandSend(str, mIdReq);
	if (!ok)
		return procErrLog(-1, "could not send command");

	return Pending;
}

Success RemoteCommanding::responseReceive()
{
	string resp, str, msg;
	bool ok;

	ok = SingleWireScheduling::commandResponseGet(mIdReq, resp);
	if (!ok)
		return Pending;

	//procWrnLog("response received: '%s'", resp.c_str());

	if (mModeAuto)
	{
		lfToCrLf(resp.data(), str);

		if (!str.size() || str.back() != '\n')
			str += "\r\n";

		mpTrans->send(str.c_str(), str.size());

		return Positive;
	}

	if (mTimestamps)
	{
		msg += dColorGrey;
		msg += nowToStr("%H:%M:%S ");
		msg += dColorClear;
	}

	if (!resp.size())
	{
		msg += dColorGrey;
		msg += "<done>";
		msg += dColorClear;
	}
	else
	{
		lfToCrLf(resp.data(), str);
		msg += str;
	}

	if (msg.size())
		msg += "\r\n";

	mpFilt->send(msg.c_str(), msg.size());
	promptSend();

	mDelayResponseCmdMs = millis() - mStartCmdMs;

	return Positive;
}

void RemoteCommanding::lineAck()
{
	promptSend(false, false, true);

	mTxtPrompt.ustrWorkSet(U"");

	historyUpdate();
}

void RemoteCommanding::historyUpdate()
{
	miEntryHist = mHistory.end();

	if (!mCmdLast.size())
		return;

	// ignore duplicate
	if (mHistory.size() && mCmdLast == mHistory.back())
		return;

	mHistory.push_back(mCmdLast);

	while (mHistory.size() > dSizeHistoryMax)
		mHistory.pop_front();
}

bool RemoteCommanding::historyNavigate(const KeyUser &key)
{
	if (key != keyUp && key != keyDown)
		return false;

	if (!mHistory.size())
		return false;

	list<u32string>::iterator iter = miEntryHist;

	if (key == keyUp && miEntryHist != mHistory.begin())
		--miEntryHist;

	if (key == keyDown && miEntryHist != mHistory.end())
		++miEntryHist;

	if (iter == miEntryHist)
		return false;

	if (miEntryHist == mHistory.end())
		mTxtPrompt.ustrWorkSet(U"");
	else
		mTxtPrompt.ustrWorkSet(*miEntryHist);

	return true;
}

void RemoteCommanding::tabProcess()
{
	uint32_t cursorFront = mTxtPrompt.cursorAbsFront();
	uint32_t cursorBack = mTxtPrompt.cursorAbsBack();

	mCursorEditLow = cursorFront > cursorBack
					? cursorBack : cursorFront;

	mStrEdit = mTxtPrompt.ustrWork();

	if (!mStrEdit.size())
		return;

	if (mLastKeyWasTab)
	{
		cmdCandidatesShow();
		return;
	}

	cmdAutoComplete();
	mLastKeyWasTab = true;
}

void RemoteCommanding::cmdAutoComplete()
{
	list<const char32_t *> candidates;
	list<const char32_t *>::const_iterator iter;
	const char32_t *pNext;
	const char32_t *pCandidateEnd;
	uint16_t idxEnd = mCursorEditLow;
	u32string ustrPatch;
	string strPatch;

	cmdCandidatesGet(candidates);

	while (true)
	{
		pNext = NULL;

		iter = candidates.begin();
		for (; iter != candidates.end(); ++iter)
		{
			pCandidateEnd = *iter + idxEnd;

			if (!pNext)
			{
				pNext = pCandidateEnd;
				continue;
			}

			if (*pCandidateEnd == *pNext)
				continue;

			pNext = NULL;
			break;
		}

		if (!pNext)
			break;

		if (!*pNext)
		{
			ustrPatch.push_back(U' ');
			break;
		}

		ustrPatch.push_back(*pNext);

		++idxEnd;
	}

	utfToStr(ustrPatch, strPatch);
	mTxtPrompt.paste(strPatch);

	promptSend();
}

void RemoteCommanding::cmdCandidatesShow()
{
	list<const char32_t *> candidates;
	list<const char32_t *>::const_iterator iter;
	size_t widthNameCmdMax = 20;
	uint8_t idxColCmdMax = 1;
	uint8_t idxColCmd = 0;
	u32string ustr, ustr2;
	string str, msg;

	cmdCandidatesGet(candidates);

	if (!candidates.size())
		return;

	promptSend(false, false, true);

	iter = candidates.begin();
	for (; iter != candidates.end(); ++iter)
	{
		ustr2 = *iter;
		ustr = ustr2.substr(0, widthNameCmdMax);

		if (ustr.size() < widthNameCmdMax)
			ustr += u32string(widthNameCmdMax - ustr.size(), U' ');

		ustr += U"  ";

		utfToStr(ustr, str);
		msg += str;

		if (idxColCmd < idxColCmdMax)
		{
			++idxColCmd;
			continue;
		}

		msg += "\r\n";
		mpFilt->send(msg.c_str(), msg.size());

		idxColCmd = 0;
		msg = "";
	}

	if (msg.size())
	{
		msg += "\r\n";
		mpFilt->send(msg.c_str(), msg.size());
	}

	promptSend();
}

void RemoteCommanding::cmdCandidatesGet(list<const char32_t *> &listCandidates)
{
	list<EntryHelp>::const_iterator iter;
	const char32_t *pEdit, *pId;

	u32string ustr = mTxtPrompt.ustrWork();
	pEdit = &ustr[0];

	size_t numBytesCheck = mCursorEditLow * sizeof(char32_t);

	iter = cmds.begin();
	for (; iter != cmds.end(); ++iter)
	{
		pId = &iter->id[0];

		if (memcmp(pEdit, pId, numBytesCheck))
			continue;

		listCandidates.push_back(pId);
	}
}

void RemoteCommanding::promptSend(bool cursor, bool preNewLine, bool postNewLine)
{
	string msg;

	if (preNewLine)
		msg += "\r\n";

	msg += "\rcore@";

	if (!mTargetIsOnline && !postNewLine)
		msg += dColorOrange;

	if (mTargetIsOnline && !postNewLine)
		msg += dColorGreen;

	msg += "remote";
	msg += dColorClear;

	msg += ":";
	msg += "~"; // directory
	msg += "# ";

	if (!mTargetIsOnline && !postNewLine &&
			!mTxtPrompt.sizeDisplayed())
	{
		msg += dColorGrey;
		msg += "<target offline>";
		msg += dColorClear;
	}
	else
	{
		mTxtPrompt.cursorShow(cursor);
		mTxtPrompt.print(msg);
	}

	if (postNewLine)
		msg += "\r\n";

	mpFilt->send(msg.c_str(), msg.size());
}

void RemoteCommanding::cmdHelpPrint(char *pArgs, char *pBuf, char *pBufEnd)
{
	list<EntryHelp>::iterator iter;
	EntryHelp cmd;
	string group = "";
	string str;
	size_t szColCmd;

	(void)pArgs;

	dInfo("\nAvailable commands\n");

	iter = cmds.begin();
	for (; iter != cmds.end(); ++iter)
	{
		cmd = *iter;

		if (cmd.group != group)
		{
			dInfo("\n");

			if (cmd.group.size() && cmd.group != cInternalCmdCls)
				dInfo("%s\n", cmd.group.c_str());
			group = cmd.group;
		}

		dInfo("  ");

		szColCmd = cmd.shortcut.size();

		if (szColCmd)
		{
			utfToStr(cmd.shortcut, str);
			dInfo("%s, ", str.c_str());

			szColCmd += 2;
		}
		else
		{
			dInfo("   ");
			szColCmd = 3;
		}

		szColCmd += cmd.id.size();

		utfToStr(cmd.id, str);
		dInfo("%s ", str.c_str());

		for (size_t i = szColCmd; i < cSizeColCmdMax; ++i)
			dInfo(" ");

		if (cmd.desc.size())
			dInfo(".. %s", cmd.desc.c_str());

		dInfo("\n");
	}
}

void RemoteCommanding::processInfo(char *pBuf, char *pBufEnd)
{
#if 0
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
#endif
	string str;

	dInfo("Last command\t\t");
	if (mCmdLast.size())
	{
		utfToStr(mCmdLast, str);
		dInfo("%s\n", str.c_str());
	}
	else
		dInfo("<none>\n");

	dInfo("Command delay\t\t%u [ms]\n", mDelayResponseCmdMs);
	dInfo("Command history\t\t%zu\n", mHistory.size());
#if 0
	list<u32string>::iterator iter;
	size_t idxHist = 0;

	if (!mHistory.size())
		dInfo("  <none>\n");

	iter = mHistory.begin();
	for (; iter != mHistory.end(); ++iter)
	{
		utfToStr(*iter, str);
		dInfo("%c %s\n",
				iter == miEntryHist ? '>' : ' ',
				str.c_str());

		if (idxHist >= 4)
			break;
		++idxHist;
	}
#endif
}

/* static functions */

void RemoteCommanding::listCommandsUpdate(const list<string> &listStr)
{
	list<string>::const_iterator iter;
	vector<string> partsEntry;
	EntryHelp entry;
	u32string ustr;

	cmds.clear();

	iter = listStr.begin();
	for (; iter != listStr.end(); ++iter)
	{
		const string &str = *iter;

		if (!str.size())
			continue;

		if (str == "infoHelp|||")
			continue;

		if (str == "levelLogSys|||")
			continue;

		//wrnLog("entry received: %s", str.c_str());

		partsEntry = split(str, '|');
		if (partsEntry.size() != 4)
		{
			wrnLog("wrong number of parts for entry: %zu", partsEntry.size());
			wrnLog("entry: %s", str.c_str());
			continue;
		}

		strToUtf(partsEntry[0], ustr);
		entry.id = ustr;

		strToUtf(partsEntry[1], ustr);
		entry.shortcut = ustr;

		entry.desc = partsEntry[2];
		entry.group = partsEntry[3];

		cmds.push_back(entry);
	}

	entry.id = U"help";
	entry.shortcut = U"h";
	entry.desc = "This help screen";
	entry.group = cInternalCmdCls;
	cmds.push_back(entry);

	entry.id = U"timestampsToggle";
	entry.shortcut = U"";
	entry.desc = "Print timestamps";
	entry.group = cInternalCmdCls;
	cmds.push_back(entry);

	entry.id = U"levelLogSys";
	entry.shortcut = U"";
	entry.desc = "Set the log level";
	entry.group = cInternalCmdCls;
	cmds.push_back(entry);

	entry.id = U"monitoringToggle";
	entry.shortcut = U"";
	entry.desc = "Cyclic check for new data";
	entry.group = cInternalCmdCls;
	cmds.push_back(entry);

	cmds.sort(commandSort);
}

bool RemoteCommanding::commandSort(const EntryHelp &cmdFirst, const EntryHelp &cmdSecond)
{
	if (cmdFirst.group == cInternalCmdCls && cmdSecond.group != cInternalCmdCls)
		return true;
	if (cmdFirst.group != cInternalCmdCls && cmdSecond.group == cInternalCmdCls)
		return false;

	if (cmdFirst.group < cmdSecond.group)
		return true;
	if (cmdFirst.group > cmdSecond.group)
		return false;

	if (cmdFirst.shortcut.size() && !cmdSecond.shortcut.size())
		return true;
	if (!cmdFirst.shortcut.size() && cmdSecond.shortcut.size())
		return false;

	if (cmdFirst.id < cmdSecond.id)
		return true;
	if (cmdFirst.id > cmdSecond.id)
		return false;

	return true;
}

vector<string> RemoteCommanding::split(const string &str, char delimiter)
{
	vector<string> result;
	stringstream ss(str);
	string item;

	while (getline(ss, item, delimiter))
		result.push_back(item);

	if (str.size() && str.back() == '|')
		result.push_back("");

	return result;
}

void RemoteCommanding::lfToCrLf(const char *pBuf, string &str)
{
	const char *pBufLineStart, *pBufIter;
	const char *pBufEnd;
	size_t lenBuf;

	str.clear();

	if (!pBuf || !*pBuf)
		return;

	lenBuf = strlen(pBuf);
	str.reserve(lenBuf << 1);

	pBufEnd = pBuf + lenBuf;
	pBufLineStart = pBufIter = pBuf;

	while (1)
	{
		if (pBufIter >= pBufEnd)
			break;

		if (*pBufIter != '\n')
		{
			++pBufIter;
			continue;
		}

		str += string(pBufLineStart, (size_t)(pBufIter - pBufLineStart));
		str += "\r\n";

		++pBufIter;
		pBufLineStart = pBufIter;
	}

	str += pBufLineStart;
}

