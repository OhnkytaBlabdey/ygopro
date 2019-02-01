#include "single_mode.h"
#include "duelclient.h"
#include "game.h"
#include <random>

namespace ygo {

long SingleMode::pduel = 0;
bool SingleMode::is_closing = false;
bool SingleMode::is_continuing = false;
Replay SingleMode::last_replay;
Replay SingleMode::new_replay;
std::vector<ReplayPacket> SingleMode::replay_stream;

bool SingleMode::StartPlay() {
	std::thread(SinglePlayThread).detach();
	return true;
}
void SingleMode::StopPlay(bool is_exiting) {
	is_closing = is_exiting;
	is_continuing = false;
	mainGame->actionSignal.Set();
	mainGame->singleSignal.Set();
}
void SingleMode::SetResponse(unsigned char* resp, unsigned int len) {
	if(!pduel)
		return;
	last_replay.WriteInt8(len);
	last_replay.WriteData(resp, len);
	set_responseb(pduel, resp);
}
int SingleMode::SinglePlayThread() {
	const int start_lp = 8000;
	const int start_hand = 5;
	const int draw_count = 1;
	const int opt = 0;
	time_t seed = time(0);
	DuelClient::rnd.seed(seed);
	pduel = mainGame->SetupDuel(DuelClient::rnd());
	mainGame->dInfo.lua64 = true;
	set_player_info(pduel, 0, start_lp, start_hand, draw_count);
	set_player_info(pduel, 1, start_lp, start_hand, draw_count);
	mainGame->dInfo.lp[0] = start_lp;
	mainGame->dInfo.lp[1] = start_lp;
	mainGame->dInfo.startlp = start_lp;
	mainGame->dInfo.strLP[0] = fmt::to_wstring(mainGame->dInfo.lp[0]);
	mainGame->dInfo.strLP[1] = fmt::to_wstring(mainGame->dInfo.lp[1]);
	mainGame->dInfo.hostname[0] = mainGame->ebNickName->getText();
	mainGame->dInfo.clientname[0] = L"";
	mainGame->dInfo.player_type = 0;
	mainGame->dInfo.turn = 0;
	bool loaded = true;
	std::string script_name = "";
	if(open_file) {
		open_file = false;
		script_name = BufferIO::EncodeUTF8s(open_file_name);
		if(!preload_script(pduel, (char*)script_name.c_str(), 0, 0, nullptr)) {
			script_name = BufferIO::EncodeUTF8s(L"./single/" + open_file_name);
			if(!preload_script(pduel, (char*)script_name.c_str(), 0, 0, nullptr))
				loaded = false;
		}
	} else {
		const std::wstring name = mainGame->lstSinglePlayList->getListItem(mainGame->lstSinglePlayList->getSelected());
		script_name = BufferIO::EncodeUTF8s(L"./single/" + name);
		if(!preload_script(pduel, (char*)script_name.c_str(), 0, 0, nullptr))
			loaded = false;
	}
	if(!loaded) {
		end_duel(pduel);
		return 0;
	}
	ReplayHeader rh;
	rh.id = 0x31707279;
	rh.version = PRO_VERSION;
	rh.flag = REPLAY_SINGLE_MODE + REPLAY_LUA64;
	rh.seed = seed;
	mainGame->gMutex.lock();
	mainGame->HideElement(mainGame->wSinglePlay);
	mainGame->ClearCardInfo();
	mainGame->wCardImg->setVisible(true);
	mainGame->wInfos->setVisible(true);
	mainGame->btnLeaveGame->setVisible(true);
	mainGame->btnLeaveGame->setText(dataManager.GetSysString(1210).c_str());
	mainGame->wPhase->setVisible(true);
	mainGame->dField.Clear();
	mainGame->dInfo.isFirst = true;
	mainGame->dInfo.isStarted = true;
	mainGame->dInfo.isSingleMode = true;
	mainGame->device->setEventReceiver(&mainGame->dField);
	mainGame->gMutex.unlock();
	char engineBuffer[0x20000];
	is_closing = false;
	is_continuing = true;
	last_replay.BeginRecord(false);
	last_replay.WriteHeader(rh);
	//records the replay with the new system
	new_replay.BeginRecord();
	rh.id = 0x58707279;
	rh.flag |= REPLAY_NEWREPLAY;
	new_replay.WriteHeader(rh);
	replay_stream.clear();
	unsigned short buffer[20];
	BufferIO::CopyWStr(mainGame->dInfo.hostname[0].c_str(), buffer, 20);
	last_replay.WriteData(buffer, 40, false);
	new_replay.WriteData(buffer, 40, false);
	BufferIO::CopyWStr(mainGame->dInfo.clientname[0].c_str(), buffer, 20);
	last_replay.WriteData(buffer, 40, false);
	new_replay.WriteData(buffer, 40, false);
	last_replay.WriteInt32(start_lp, false);
	last_replay.WriteInt32(start_hand, false);
	last_replay.WriteInt32(draw_count, false);
	last_replay.WriteInt32(opt, false);
	last_replay.WriteInt16(script_name.size(), false);
	last_replay.WriteData(script_name.c_str(), script_name.size(), false);
	last_replay.Flush();
	new_replay.WriteInt32((mainGame->GetMasterRule(opt, 0)) | (opt & SPEED_DUEL) << 8);
	int len = get_message(pduel, (byte*)engineBuffer);
	if (len > 0){
		is_continuing = SinglePlayAnalyze(engineBuffer, len);
	}
	start_duel(pduel, opt);
	while (is_continuing) {
		int result = process(pduel);
		len = result & 0xffff;
		/* int flag = result >> 16; */
		if (len > 0) {
			get_message(pduel, (byte*)engineBuffer);
			is_continuing = SinglePlayAnalyze(engineBuffer, len);
		}
	}
	last_replay.EndRecord(0x1000);
	char replaybuf[0x2000], *pbuf = replaybuf;
	memcpy(pbuf, &last_replay.pheader, sizeof(ReplayHeader));
	pbuf += sizeof(ReplayHeader);
	memcpy(pbuf, last_replay.comp_data, last_replay.comp_size);

	new_replay.WritePacket(ReplayPacket(OLD_REPLAY_MODE, replaybuf, sizeof(ReplayHeader) + last_replay.comp_size));

	new_replay.EndRecord();
	time_t nowtime = time(NULL);
	tm* localedtime = localtime(&nowtime);
	wchar_t timetext[40];
	wcsftime(timetext, 40, L"%Y-%m-%d %H-%M-%S", localedtime);
	mainGame->gMutex.lock();
	mainGame->ebRSName->setText(timetext);
	mainGame->wReplaySave->setText(dataManager.GetSysString(1340).c_str());
	mainGame->PopupElement(mainGame->wReplaySave);
	mainGame->gMutex.unlock();
	mainGame->replaySignal.Reset();
	mainGame->replaySignal.Wait();
	if(mainGame->saveReplay)
		new_replay.SaveReplay(mainGame->ebRSName->getText());
	end_duel(pduel);
	if(!is_closing) {
		mainGame->gMutex.lock();
		mainGame->dInfo.isStarted = false;
		mainGame->dInfo.isSingleMode = false;
		mainGame->gMutex.unlock();
		mainGame->closeDoneSignal.Reset();
		mainGame->closeSignal.lock();
		mainGame->closeDoneSignal.Wait();
		mainGame->closeSignal.unlock();
		mainGame->gMutex.lock();
		mainGame->ShowElement(mainGame->wSinglePlay);
		mainGame->stTip->setVisible(false);
		mainGame->device->setEventReceiver(&mainGame->menuHandler);
		mainGame->gMutex.unlock();
		if(exit_on_return)
			mainGame->device->closeDevice();
	}
	return 0;
}
bool SingleMode::SinglePlayAnalyze(char* msg, unsigned int len) {
	char* offset, *pbuf = msg;
	int player, count;
	while (pbuf - msg < (int)len) {
		replay_stream.clear();
		bool record = true;
		if(is_closing || !is_continuing)
			return false;
		offset = pbuf;
		ReplayPacket p;
		mainGame->dInfo.curMsg = BufferIO::ReadUInt8(pbuf);
		p.message = mainGame->dInfo.curMsg;
		p.length = len - 1;
		memcpy(p.data, pbuf, p.length);
		switch (mainGame->dInfo.curMsg) {
		case MSG_RETRY: {
			mainGame->gMutex.lock();
			mainGame->stMessage->setText(L"Error occurs.");
			mainGame->PopupElement(mainGame->wMessage);
			mainGame->gMutex.unlock();
			mainGame->actionSignal.Reset();
			mainGame->actionSignal.Wait();
			return false;
		}
		case MSG_HINT: {
			int type = BufferIO::ReadInt8(pbuf);
			int player = BufferIO::ReadInt8(pbuf);
			/*int data = */BufferIO::ReadInt64(pbuf);
			if(player == 0)
				DuelClient::ClientAnalyze(offset, pbuf - offset);
			if (type > 0 && type < 6 && type != 4)
				record = false;
			break;
		}
		case MSG_WIN: {
			pbuf += 2;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			replay_stream.push_back(p);
			return false;
		}
		case MSG_SELECT_BATTLECMD: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 18;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 8 + 2;
			SinglePlayRefresh();
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_IDLECMD: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 18 + 3;
			SinglePlayRefresh();
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_EFFECTYN: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 22;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_YESNO: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 8;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_OPTION: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 8;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_CARD: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 3;
			count = BufferIO::ReadInt32(pbuf);
			pbuf += count * 14;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_TRIBUTE: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 3;
			count = BufferIO::ReadInt32(pbuf);
			pbuf += count * 11;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_UNSELECT_CARD: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 4;
			count = BufferIO::ReadInt32(pbuf);
			pbuf += count * 14;
			count = BufferIO::ReadInt32(pbuf);
			pbuf += count * 14;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_CHAIN: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += 10 + count * 23;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_PLACE:
		case MSG_SELECT_DISFIELD: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 5;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_POSITION: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 5;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_COUNTER: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 4;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 9;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SELECT_SUM: {
			pbuf++;
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 6;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 14;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 14;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_SORT_CARD:
		case MSG_SORT_CHAIN: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 19;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_CONFIRM_DECKTOP: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CONFIRM_EXTRATOP: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CONFIRM_CARDS: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SHUFFLE_DECK: {
			player = BufferIO::ReadInt8(pbuf);
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh(player, LOCATION_DECK, 0x181fff);
			break;
		}
		case MSG_SHUFFLE_HAND: {
			/*int oplayer = */BufferIO::ReadInt8(pbuf);
			int count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SHUFFLE_EXTRA: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_REFRESH_DECK: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SWAP_GRAVE_DECK: {
			player = BufferIO::ReadInt8(pbuf);
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh(player, LOCATION_GRAVE, 0x181fff);
			break;
		}
		case MSG_REVERSE_DECK: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh(0, LOCATION_DECK, 0x181fff);
			SinglePlayRefresh(1, LOCATION_DECK, 0x181fff);
			break;
		}
		case MSG_DECK_TOP: {
			pbuf += 6;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SHUFFLE_SET_CARD: {
			pbuf++;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 20;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_NEW_TURN: {
			player = BufferIO::ReadInt8(pbuf);
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_NEW_PHASE: {
			pbuf += 2;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			break;
		}
		case MSG_MOVE: {
			pbuf += 4;
			loc_info previous = ClientCard::read_location_info(pbuf);
			loc_info current = ClientCard::read_location_info(pbuf);
			pbuf += 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			if(previous.location && !(current.location & 0x80) && (previous.location != current.location || previous.controler != current.controler))
				SinglePlayRefreshSingle(current.controler, current.location, current.sequence);
			break;
		}
		case MSG_POS_CHANGE: {
			pbuf += 9;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SET: {
			pbuf += 14;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SWAP: {
			pbuf += 28;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_FIELD_DISABLED: {
			pbuf += 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SUMMONING: {
			pbuf += 14;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SUMMONED: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			break;
		}
		case MSG_SPSUMMONING: {
			pbuf += 14;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SPSUMMONED: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			break;
		}
		case MSG_FLIPSUMMONING: {
			pbuf += 14;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_FLIPSUMMONED: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			break;
		}
		case MSG_CHAINING: {
			pbuf += 26;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CHAINED: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			break;
		}
		case MSG_CHAIN_SOLVING: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CHAIN_SOLVED: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			break;
		}
		case MSG_CHAIN_END: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			SinglePlayRefresh(0, LOCATION_DECK, 0x181fff);
			SinglePlayRefresh(1, LOCATION_DECK, 0x181fff);
			break;
		}
		case MSG_CHAIN_NEGATED: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CHAIN_DISABLED: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CARD_SELECTED:
		case MSG_RANDOM_SELECTED: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_BECOME_TARGET: {
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 10;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_DRAW: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_DAMAGE: {
			pbuf += 5;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_RECOVER: {
			pbuf += 5;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_EQUIP: {
			pbuf += 20;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_LPUPDATE: {
			pbuf += 5;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_UNEQUIP: {
			pbuf += 10;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CARD_TARGET: {
			pbuf += 20;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CANCEL_TARGET: {
			pbuf += 20;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_PAY_LPCOST: {
			pbuf += 5;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_ADD_COUNTER: {
			pbuf += 7;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_REMOVE_COUNTER: {
			pbuf += 7;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_ATTACK: {
			pbuf += 20;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_BATTLE: {
			pbuf += 38;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_ATTACK_DISABLED: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_DAMAGE_STEP_START: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			break;
		}
		case MSG_DAMAGE_STEP_END: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh();
			break;
		}
		case MSG_MISSED_EFFECT: {
			pbuf += 14;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_TOSS_COIN: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_TOSS_DICE: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_ROCK_PAPER_SCISSORS: {
			player = BufferIO::ReadInt8(pbuf);
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_HAND_RES: {
			pbuf += 1;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_ANNOUNCE_RACE: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 5;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_ANNOUNCE_ATTRIB: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 5;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_ANNOUNCE_CARD: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 4;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_ANNOUNCE_NUMBER:
		case MSG_ANNOUNCE_CARD_FILTER: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += 8 * count;
			if(!DuelClient::ClientAnalyze(offset, pbuf - offset)) {
				mainGame->singleSignal.Reset();
				mainGame->singleSignal.Wait();
			}
			record = false;
			break;
		}
		case MSG_CARD_HINT: {
			pbuf += 19;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_PLAYER_HINT: {
			pbuf += 10;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_TAG_SWAP: {
			player = pbuf[0];
			pbuf += pbuf[2] * 4 + pbuf[4] * 4 + 9;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayRefresh(player, LOCATION_DECK, 0x181fff);
			SinglePlayRefresh(player, LOCATION_EXTRA, 0x181fff);
			break;
		}
		case MSG_MATCH_KILL: {
			pbuf += 4;
			break;
		}
		case MSG_RELOAD_FIELD: {
			pbuf++;
			for(int p = 0; p < 2; ++p) {
				pbuf += 4;
				for(int seq = 0; seq < 7; ++seq) {
					int val = BufferIO::ReadInt8(pbuf);
					if(val)
						pbuf += 2;
				}
				for(int seq = 0; seq < 8; ++seq) {
					int val = BufferIO::ReadInt8(pbuf);
					if(val)
						pbuf++;
				}
				pbuf += 11;
			}
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			SinglePlayReload();
			mainGame->gMutex.lock();
			mainGame->dField.RefreshAllCards();
			mainGame->gMutex.unlock();
			break;
		}
		case MSG_AI_NAME: {
			int len = BufferIO::ReadInt16(pbuf);
			char* begin = pbuf;
			pbuf += len + 1;
			std::string namebuf;
			namebuf.resize(len);
			memcpy(&namebuf[0], begin, len + 1);
			mainGame->dInfo.clientname[0] = BufferIO::DecodeUTF8s(namebuf);
			break;
		}
		case MSG_SHOW_HINT: {
			char msgbuf[1024];
			wchar_t msg[1024];
			int len = BufferIO::ReadInt16(pbuf);
			char* begin = pbuf;
			pbuf += len + 1;
			memcpy(msgbuf, begin, len + 1);
			BufferIO::DecodeUTF8(msgbuf, msg);
			mainGame->gMutex.lock();
			mainGame->stMessage->setText(msg);
			mainGame->PopupElement(mainGame->wMessage);
			mainGame->gMutex.unlock();
			mainGame->actionSignal.Reset();
			mainGame->actionSignal.Wait();
			break;
		}
		}
		//setting the length again in case of multiple messages in a row,
		//when the packet will be written in the replay, the extra data registered previously will be discarded
		p.length = (pbuf - offset) - 1;
		if (record)
			replay_stream.insert(replay_stream.begin(), p);
		new_replay.WriteStream(replay_stream);
	}
	return is_continuing;
}
void SingleMode::SinglePlayRefresh(int player, int location, int flag) {
	unsigned char queryBuffer[0x20000];
	char queryBuffer2[0x20000];
	char* qbuf = queryBuffer2;
	ReplayPacket p;
	int len = query_field_card(pduel, player, location, flag, queryBuffer, TRUE, FALSE);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(player), location, (char*)queryBuffer);
	BufferIO::WriteInt8(qbuf, player);
	BufferIO::WriteInt8(qbuf, location);
	memcpy(qbuf, (char*)queryBuffer, len);
	replay_stream.push_back(ReplayPacket(MSG_UPDATE_DATA, queryBuffer2, len + 2));
}
void SingleMode::SinglePlayRefreshSingle(int player, int location, int sequence, int flag) {
	unsigned char queryBuffer[0x2000];
	char queryBuffer2[0x2000];
	char* qbuf = queryBuffer2;
	int len = query_card(pduel, player, location, sequence, flag, queryBuffer, TRUE, FALSE);
	mainGame->dField.UpdateCard(mainGame->LocalPlayer(player), location, sequence, (char*)queryBuffer);
	BufferIO::WriteInt8(qbuf, player);
	BufferIO::WriteInt8(qbuf, location);
	BufferIO::WriteInt8(qbuf, sequence);
	memcpy(qbuf, (char*)queryBuffer, len);
	ReplayPacket p(MSG_UPDATE_CARD, queryBuffer2, len + 3);
	replay_stream.push_back(p);
}
void SingleMode::SinglePlayRefresh(int flag) {
	for(int p = 0; p < 2; p++)
		for(int loc = LOCATION_HAND; loc != LOCATION_GRAVE; loc *= 2)
			SinglePlayRefresh(p, loc, flag);
}
void SingleMode::SinglePlayReload() {
	for(int p = 0; p < 2; p++)
		for(int loc = LOCATION_DECK; loc != LOCATION_OVERLAY; loc *= 2)
			SinglePlayRefresh(p, loc, 0xffdfff);
}

}