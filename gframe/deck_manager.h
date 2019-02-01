#ifndef DECKMANAGER_H
#define DECKMANAGER_H

#include "config.h"
#include "client_card.h"
#include <unordered_map>
#include <vector>

namespace ygo {

struct LFList {
	unsigned int hash;
	std::wstring listName;
	std::unordered_map<int, int> content;
	bool whitelist;
};
struct Deck {
	std::vector<code_pointer> main;
	std::vector<code_pointer> extra;
	std::vector<code_pointer> side;
	Deck() {}
	Deck(const Deck& ndeck) {
		main = ndeck.main;
		extra = ndeck.extra;
		side = ndeck.side;
	}
	void clear() {
		main.clear();
		extra.clear();
		side.clear();
	}
};

class DeckManager {
public:
	Deck current_deck;
	Deck pre_deck;
	std::vector<LFList> _lfList;

	void LoadLFListSingle(const char* path);
	void LoadLFList();
	std::wstring GetLFListName(int lfhash);
	int CheckDeck(Deck& deck, int lfhash, bool allow_ocg, bool allow_tcg, bool doubled, int forbiddentypes = 0);
	int TypeCount(std::vector<code_pointer> cards, int type);
	int LoadDeck(Deck& deck, int* dbuf, int mainc, int sidec, int mainc2 = 0, int sidec2 = 0);
	int LoadDeck(Deck& deck, std::vector<int> mainlist, std::vector<int> sidelist);
	bool LoadSide(Deck& deck, int* dbuf, int mainc, int sidec);
	bool LoadDeck(const std::wstring& file, Deck* deck = nullptr);
	bool LoadDeckDouble(const std::wstring& file, const std::wstring& file2, Deck* deck = nullptr);
	bool SaveDeck(Deck& deck, const std::wstring& name);
	bool DeleteDeck(Deck& deck, const std::wstring& name);
};

extern DeckManager deckManager;

}

#endif //DECKMANAGER_H
