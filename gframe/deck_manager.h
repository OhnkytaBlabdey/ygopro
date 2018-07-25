#ifndef DECKMANAGER_H
#define DECKMANAGER_H

#include "config.h"
#include "client_card.h"
#include <unordered_map>
#include <vector>

namespace ygo {

struct LFList {
	unsigned int hash;
	wchar_t listName[20];
	std::unordered_map<int, int>* content;
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

	void LoadLFList();
	wchar_t* GetLFListName(int lfhash);
	int CheckDeck(Deck& deck, int lfhash, bool allow_ocg, bool allow_tcg, bool doubled, int forbiddentypes = 0);
	int TypeCount(std::vector<code_pointer> cards, int type);
	int LoadDeck(Deck& deck, int* dbuf, int mainc, int sidec, int mainc2 = 0, int sidec2 = 0, bool doubled = false);
	bool LoadSide(Deck& deck, int* dbuf, int mainc, int sidec);
	FILE* OpenDeckFile(const wchar_t * file, const char * mode);
	bool LoadDeck(const wchar_t* file);
	bool LoadDeckDouble(const wchar_t* file, const wchar_t* file2);
	bool SaveDeck(Deck& deck, const wchar_t* name);
	bool DeleteDeck(Deck& deck, const wchar_t* name);
};

extern DeckManager deckManager;

}

#endif //DECKMANAGER_H
