#include "shami.h"
// #include <fcitx-utills/utf8.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/userinterfacemanager.h>
#include "boshiamy_data.hpp"
#include <utility>

namespace {

const int WORDS_PER_PAGE = 10;
const static std::string chewing_prefix = "\';";

static const std::array<fcitx::Key, WORDS_PER_PAGE> selectionKeys = {
    fcitx::Key{FcitxKey_1}, fcitx::Key{FcitxKey_2}, fcitx::Key{FcitxKey_3},
    fcitx::Key{FcitxKey_4}, fcitx::Key{FcitxKey_5}, fcitx::Key{FcitxKey_6},
    fcitx::Key{FcitxKey_7}, fcitx::Key{FcitxKey_8}, fcitx::Key{FcitxKey_9},
    fcitx::Key{FcitxKey_0}
};

static const std::string boshiamyKeys = ";\',.[]abcdefghijklmnopqrstuvwxyz";

bool isBoshiamyKey(fcitx::KeyEvent &event) {
	return event.key().isSimple() && 
		boshiamyKeys.find(event.key().sym()) < boshiamyKeys.size();
}

class ShamiCandidateWord: public fcitx::CandidateWord {
public:
	ShamiCandidateWord(ShamiEngine* engine, std::string text)
		: engine_(engine) , ctx_(text){
			std::string shown_text = 
				ctx_+"\n"+BoshiamyData::get_shortest(ctx_);
			setText(fcitx::Text(shown_text));
		}
	void select(fcitx::InputContext* inputContext) const override {
		FCITX_INFO() << "selected " << ctx_;
		inputContext -> commitString(ctx_);
		auto state = inputContext -> propertyFor(engine_ -> factory());
		state -> reset();
	}
private:
	ShamiEngine* engine_;
	std::string ctx_;
};


// Only works if there exists at least one candidate!!!
class ShamiBoshiamyCandidateList: public fcitx::CandidateList,
						  public fcitx::PageableCandidateList, 
						  public fcitx::CursorMovableCandidateList, 
						  public fcitx::CursorModifiableCandidateList {
public:
	ShamiBoshiamyCandidateList(ShamiEngine* engine, fcitx::InputContext* ic,
			const std::string text): engine_(engine), ic_(ic), text_(text) {
		setPageable(this);
		setCursorMovable(this);
		setCursorModifiable(this);
		for(int i = 0;i<WORDS_PER_PAGE;i++) {
			const char label[2] = {static_cast<char>('0' + (i + 1) % WORDS_PER_PAGE), '\0'};
			labels_[i].append(label);
			labels_[i].append(". ");
		}
		generate();
	}
	const fcitx::Text &label(int idx) const override {
		return labels_[idx];
	}
	const fcitx::CandidateWord& candidate(int idx) const override {
		return *candidates_[idx];
	}
	int size() const override {
		return size_;
	}
	fcitx::CandidateLayoutHint layoutHint() const override {
		return fcitx::CandidateLayoutHint::Horizontal;
	}
	bool usedNextBefore() const override { return false; }
	void prev() override {
		if (!hasPrev()) {
			return;
		}
		ptr_ -= WORDS_PER_PAGE;
		setCursorIndex(0);
		updateCandidateList();
	}
	void next() override {
		if (!hasNext()) {
			return;
		}
		ptr_ += WORDS_PER_PAGE;
		setCursorIndex(0);
		updateCandidateList();
	}
	void updateCandidateList() {
		size_ = 0;
		for(int i = 0;ptr_ + i < all_.size() && i < WORDS_PER_PAGE;i++, size_++) {
			candidates_[i] = 
				std::make_unique<ShamiCandidateWord>(engine_, all_[ptr_ + i]);
		}
	}
	bool hasPrev() const override {
		if (ptr_ - WORDS_PER_PAGE >= 0) return true;
		else return false;
	}
	bool hasNext() const override {
		if (ptr_ + WORDS_PER_PAGE < all_.size()) return true;
		else return false;
	}
	void prevCandidate() override {
		setCursorIndex((cursor_ + size() - 1) % size());
	}
	void nextCandidate() override {
		setCursorIndex((cursor_ + 1) % size());
	}
	int cursorIndex() const override {
		return cursor_;
	}
	void setCursorIndex(int idx) {
		if (idx >= size() || idx < 0) return;
		cursor_ = idx;
		return;
	}
private:
	void generate() {
		all_ = BoshiamyData::get_typed_words(text_);
		assert(!all_.empty());
		updateCandidateList();
		return;
	}
	ShamiEngine* engine_;
	fcitx::InputContext* ic_;
	std::unique_ptr<ShamiCandidateWord> candidates_[WORDS_PER_PAGE];
	std::vector<std::string> all_;
	int ptr_ = 0;
	int cursor_ = 0;
	int size_ = 0;
	std::string text_;
	fcitx::Text labels_[WORDS_PER_PAGE];
};

class ShamiChewingCandidateList: public fcitx::CandidateList,
						  public fcitx::PageableCandidateList, 
						  public fcitx::CursorMovableCandidateList, 
						  public fcitx::CursorModifiableCandidateList {
public:
	ShamiChewingCandidateList(ShamiEngine* engine, fcitx::InputContext* ic,
			ChewingContext* chewing_ctx): engine_(engine), ic_(ic), chewing_ctx_(chewing_ctx) {
		setPageable(this);
		setCursorMovable(this);
		setCursorModifiable(this);
		for(int i = 0;i<WORDS_PER_PAGE;i++) {
			const char label[2] = {static_cast<char>('0' + (i + 1) % WORDS_PER_PAGE), '\0'};
			labels_[i].append(label);
			labels_[i].append(". ");
		}
		updateCandidateList();
	}
	const fcitx::Text &label(int idx) const override {
		return labels_[idx];
	}
	const fcitx::CandidateWord& candidate(int idx) const override {
		return *candidates_[idx];
	}
	int size() const override {
		return size_;
	}
	fcitx::CandidateLayoutHint layoutHint() const override {
		return fcitx::CandidateLayoutHint::Horizontal;
	}
	bool usedNextBefore() const override { return false; }
	void prev() override {
		chewing_handle_PageUp(chewing_ctx_);
		updateCandidateList();
	}
	void next() override {
		chewing_handle_PageDown(chewing_ctx_);
		updateCandidateList();
	}
	void updateCandidateList() {
		size_ = 0;
		int now_page = chewing_cand_CurrentPage(chewing_ctx_);
		int head = now_page * chewing_cand_ChoicePerPage(chewing_ctx_);
		int tail = std::min((now_page + 1) * chewing_cand_ChoicePerPage(chewing_ctx_), chewing_cand_TotalChoice(chewing_ctx_));
		for(int i = 0, now = head; i < 10 && now < tail; i++, now++) {
			std::string cand_word(chewing_cand_string_by_index_static(chewing_ctx_, now));
			candidates_[i] = std::make_unique<ShamiCandidateWord>(engine_, cand_word);
			size_ = i+1;
		}
	}
	bool hasPrev() const override {
		return true;
	}
	bool hasNext() const override {
		return true;
	}
	void prevCandidate() override {
		if (cursor_ == 0) {
			prev();
			setCursorIndex(0);
		}
		else {
			setCursorIndex(cursor_ - 1);
		}
		return;
	}
	void nextCandidate() override {
		if (cursor_ + 1 == size()) {
			next();
			setCursorIndex(0);
		}
		else {
			setCursorIndex(cursor_ + 1);
		}
	}
	int cursorIndex() const override {
		return cursor_;
	}
	void setCursorIndex(int idx) {
		if (idx >= size() || idx < 0) return;
		cursor_ = idx;
		return;
	}
private:
	ShamiEngine* engine_;
	fcitx::InputContext* ic_;
	std::unique_ptr<ShamiCandidateWord> candidates_[WORDS_PER_PAGE];
	int ptr_ = 0;
	int cursor_ = 0;
	int size_ = 0;
	ChewingContext* chewing_ctx_;
	fcitx::Text labels_[WORDS_PER_PAGE];
};

}

namespace {
	const int SEL_KEYS[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
	const char* CHEWING_DATA_DIR = "/usr/share/libchewing";
	const char* USER_HOME = getenv("HOME");
	std::string user_data_dir = std::string(USER_HOME) + "/.local/share/chewing.sqlite3";
}

void ShamiState::initChewing() {
	chewing_ctx_ = chewing_new2(CHEWING_DATA_DIR, user_data_dir.c_str(), NULL, 0);
	if (!chewing_ctx_) {
		FCITX_INFO() << "error initializing chewing_ctx_";
		return;
	}
	chewing_set_selKey(chewing_ctx_, SEL_KEYS, WORDS_PER_PAGE);
	chewing_set_maxChiSymbolLen(chewing_ctx_, 10);
	chewing_set_candPerPage(chewing_ctx_, WORDS_PER_PAGE);
	chewing_set_ChiEngMode(chewing_ctx_, 1);
}

bool ShamiState::handleCandidateKeyEvent(fcitx::KeyEvent &event) {
	auto candidateList = ic_ -> inputPanel().candidateList();
	if (!candidateList) return false;
	int idx = event.key().keyListIndex(selectionKeys);
	if (idx >= 0 && idx < candidateList -> size()) {
		event.accept();
		candidateList -> candidate(idx).select(ic_);
		return true;
	}
	if (event.key().check(FcitxKey_Return)) {
		event.accept();
		candidateList -> candidate(candidateList -> cursorIndex()).select(ic_);
		return true;
	}
	if (event.key().checkKeyList(engine_ -> instance() -> globalConfig().defaultPrevPage())) {
		if (auto *pageable = candidateList -> toPageable(); pageable && pageable -> hasPrev()) {
			event.accept();
			pageable -> prev();
		}
		event.filterAndAccept();
		ic_->updateUserInterface(
				fcitx::UserInterfaceComponent::InputPanel);
		return true;
	}
	if (event.key().checkKeyList(engine_ -> instance() -> globalConfig().defaultNextPage())) {
		if (auto *pageable = candidateList -> toPageable(); pageable && pageable -> hasNext()) {
			event.accept();
			pageable -> next();
		}
		event.filterAndAccept();
		ic_->updateUserInterface(
				fcitx::UserInterfaceComponent::InputPanel);
		return true;
	}
	if (event.key().checkKeyList(engine_ -> instance() -> globalConfig().defaultPrevCandidate()) || event.key().check(FcitxKey_Left)) {
		if (auto *cursormovable = candidateList -> toCursorMovable(); cursormovable) {
			event.accept();
			cursormovable -> prevCandidate();
		}
		event.filterAndAccept();
		ic_->updateUserInterface(
				fcitx::UserInterfaceComponent::InputPanel);
		return true;
	}
	if (event.key().checkKeyList(engine_ -> instance() -> globalConfig().defaultNextCandidate()) || event.key().check(FcitxKey_Right)) {
		if (auto *cursormovable = candidateList -> toCursorMovable(); cursormovable) {
			event.accept();
			cursormovable -> nextCandidate();
		}
		event.filterAndAccept();
		ic_->updateUserInterface(
				fcitx::UserInterfaceComponent::InputPanel);
		return true;
	}
	return false;
}

void ShamiState::commitBuffer() {
	auto candidateList = ic_ -> inputPanel().candidateList();
	if (!candidateList) {
		ic_ -> commitString(buffer_.userInput());
		updateUI();
	} else {
		candidateList -> candidate(candidateList -> cursorIndex()).select(ic_);
	}
	reset();
	return;
}

int ShamiState::getTypingMode() {
	std::string buffer_string = buffer_.userInput();
	if (buffer_string.size() >= chewing_prefix.size() 
			&& buffer_string.substr(0, chewing_prefix.size()) == chewing_prefix) {
		return 1;
	}
	else return 0;
}

bool ShamiState::handleBoshiamyKeyEvent(fcitx::KeyEvent &event) { // check if handled
	FCITX_INFO() << "handleBoshiamyKeyEvent in";
	if (buffer_.empty() && !isBoshiamyKey(event)) return false;
	if (event.key().check(FcitxKey_BackSpace)) {
		buffer_.backspace();
	} else if (event.key().check(FcitxKey_Return) ||
			event.key().check(FcitxKey_space)) {
		commitBuffer();
		reset();
	} else if (event.key().check(FcitxKey_Escape)) {
		reset();
	} else if (event.key().isSimple() && !event.key().isDigit()) {
		buffer_.type(event.key().sym());
	} else {
		FCITX_INFO() << "handleBoshiamyKeyEvent done";
		return false;
	}
	updateUI();
	event.filterAndAccept();
	FCITX_INFO() << "handleBoshiamyKeyEvent done";
	return true;
}

bool ShamiState::handleChewingKeyEvent(fcitx::KeyEvent &event) {
	FCITX_INFO() << "handleChewingKeyEvent in";
	bool handled_by_chewing = true;
	if (event.key().check(FcitxKey_space)) {
		if (chewing_buffer_Check(chewing_ctx_)) {
			handled_by_chewing = false;
			FCITX_INFO() << "chewing buffer unempty";
		}
		else {
			chewing_handle_Space(chewing_ctx_);
		}
	} else if (event.key().check(FcitxKey_Escape)) {
		chewing_handle_Esc(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Return)) {
		chewing_handle_Enter(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Delete)) {
		chewing_handle_Del(chewing_ctx_);
	} else if (event.key().check(FcitxKey_BackSpace)) {
		chewing_handle_Backspace(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Tab)) {
		chewing_handle_Tab(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Shift_L)) {
		chewing_handle_ShiftLeft (chewing_ctx_);
	} else if (event.key().check(FcitxKey_Left)) {
		chewing_handle_Left(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Shift_R)) {
		chewing_handle_ShiftRight(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Right)) {
		chewing_handle_Right(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Up)) {
		chewing_handle_Up(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Home)) {
		chewing_handle_Home(chewing_ctx_);
	} else if (event.key().check(FcitxKey_End)) {
		chewing_handle_End(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Page_Up)) {
		chewing_handle_PageUp(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Page_Down)) {
		chewing_handle_PageDown(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Down)) {
		chewing_handle_Down(chewing_ctx_);
	} else if (event.key().check(FcitxKey_Caps_Lock)) {
		chewing_handle_Capslock(chewing_ctx_);
	} else if (event.key().isSimple()) {
		if (chewing_buffer_Check(chewing_ctx_)) {
			handled_by_chewing = false;
			FCITX_INFO() << "chewing buffer unempty";
		}
		else {
			chewing_handle_Default(chewing_ctx_, event.key().sym());
		}
	} else {
		handled_by_chewing = false;
	}
	FCITX_INFO() << "handleChewingKeyEvent done";
	if (handled_by_chewing 
			&& !chewing_keystroke_CheckIgnore(chewing_ctx_)){
		event.filterAndAccept();
		FCITX_INFO() << "going to updateUI()";
		updateUI();
		return true;
	}
	return false;
}

void ShamiState::keyEvent(fcitx::KeyEvent &event) { //since both may exist simultanuously, we have to check both
	FCITX_INFO() << "keyevent: " << event.key();
	if (getTypingMode() == 0) { // boshiamy mode
		if (!handleBoshiamyKeyEvent(event) && !buffer_.empty()) 
			handleCandidateKeyEvent(event);
	}
	else {
		if (chewing_cand_CheckDone(chewing_ctx_)) {
			FCITX_INFO() << "handling chewing";
			if(!handleChewingKeyEvent(event)) {
				handleBoshiamyKeyEvent(event);
			}
		}
		else {
			handleCandidateKeyEvent(event);
		}
	}
}

void ShamiState::updateUI() { 
	// note that this function resets the input panel, which may cause the cursor in the candidate list to function improperly
	FCITX_INFO() << "update UI in";
	auto &inputPanel = ic_ -> inputPanel();
	inputPanel.reset();
	fcitx::Text preedit;
	if (getTypingMode() == 0) {
		if (!BoshiamyData::get_typed_words(buffer_.userInput()).empty() 
				&& !buffer_.empty()) {
			inputPanel.setCandidateList(
					std::make_unique<ShamiBoshiamyCandidateList>(
						engine_, ic_, buffer_.userInput()));
			preedit = fcitx::Text(buffer_.userInput());
		} 
	}
	else {
		FCITX_INFO() << "updating UI with chewing";
		FCITX_INFO() << "chewing bopomofo = " << chewing_bopomofo_String_static(chewing_ctx_);
		FCITX_INFO() << "chewing buffer = " << chewing_buffer_String_static(chewing_ctx_);
		if (!chewing_cand_CheckDone(chewing_ctx_)) {
			inputPanel.setCandidateList(
					std::make_unique<ShamiChewingCandidateList>(
						engine_, ic_, chewing_ctx_));
		}
		preedit = fcitx::Text(chewing_prefix
			+std::string(chewing_bopomofo_String_static(chewing_ctx_))
			+std::string(chewing_buffer_String_static(chewing_ctx_)));
		if (chewing_commit_Check(chewing_ctx_)) {
			ic_ -> commitString(chewing_commit_String_static(chewing_ctx_));
			reset();
		}
	}
	if (ic_ -> capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
		preedit = fcitx::Text(preedit.toString(), 
				fcitx::TextFormatFlag::HighLight);
	}
	inputPanel.setPreedit(preedit);
	ic_ -> updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
	ic_ -> updatePreedit();
	FCITX_INFO() << "updateUI done";
}

void ShamiEngine::keyEvent(const fcitx::InputMethodEntry& entry, 
		fcitx::KeyEvent& keyEvent) {
	if (keyEvent.isRelease() || keyEvent.key().states()) {
		return;
	}
	auto ic = keyEvent.inputContext();
	auto *state = ic -> propertyFor(&factory_);
	state -> keyEvent(keyEvent);
}

void ShamiEngine::reset(const fcitx::InputMethodEntry&, 
		fcitx::InputContextEvent& event) {
	auto *state = event.inputContext() -> propertyFor(&factory_);
	state -> reset();
}

ShamiEngine::ShamiEngine(fcitx::Instance* instance): instance_(instance), factory_([this](fcitx::InputContext& ic) {
		return new ShamiState(this, &ic);
	}) {
	instance -> inputContextManager().registerProperty("shamiState", &factory_);
}

FCITX_ADDON_FACTORY(ShamiEngineFactory);
