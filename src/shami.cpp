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

static const std::array<fcitx::Key, WORDS_PER_PAGE> selectionKeys = {
    fcitx::Key{FcitxKey_1}, fcitx::Key{FcitxKey_2}, fcitx::Key{FcitxKey_3},
    fcitx::Key{FcitxKey_4}, fcitx::Key{FcitxKey_5}, fcitx::Key{FcitxKey_6},
    fcitx::Key{FcitxKey_7}, fcitx::Key{FcitxKey_8}, fcitx::Key{FcitxKey_9},
    fcitx::Key{FcitxKey_0}
};

static const std::string boshiamyKeys = "abcdefghijklmnopqrstuvwxyz[]";

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
		inputContext -> commitString(ctx_);
		auto state = inputContext -> propertyFor(engine_ -> factory());
		state -> reset();
	}
private:
	ShamiEngine* engine_;
	std::string ctx_;
};


// Only works if there exists at least one candidate!!!
class ShamiCandidateList: public fcitx::CandidateList,
						  public fcitx::PageableCandidateList, 
						  public fcitx::CursorMovableCandidateList, 
						  public fcitx::CursorModifiableCandidateList {
public:
	ShamiCandidateList(ShamiEngine* engine, fcitx::InputContext* ic,
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
		all_ = BoshiamyData::get_typed_word(text_);
		FCITX_INFO() << "#candidates = " << all_.size();
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

}

bool ShamiState::handleCandidateKeyEvent(fcitx::KeyEvent &event) {
	FCITX_INFO() << "handling candidate key event";
	auto candidateList = ic_ -> inputPanel().candidateList();
	FCITX_INFO() << "get candidate list";
	if (!candidateList) return false;
	FCITX_INFO() << "candidate list not empty";
	int idx = event.key().keyListIndex(selectionKeys);
	if (idx >= 0 && idx < candidateList -> size()) {
		event.accept();
		candidateList -> candidate(idx).select(ic_);
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

bool ShamiState::handleNormalKeyEvent(fcitx::KeyEvent &event) { // check if handled
	if (buffer_.empty() && !isBoshiamyKey(event)) return false;
	FCITX_INFO() << "handling normal key event";
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
		return false;
	}
	updateUI();
	event.filterAndAccept();
	return true;
}

void ShamiState::keyEvent(fcitx::KeyEvent &event) { //since both may exist simultanuously, we have to check both
	if (!handleNormalKeyEvent(event) && !buffer_.empty()) 
		handleCandidateKeyEvent(event);
}

void ShamiState::updateUI() {
	FCITX_INFO() << "Updating UI";
	auto &inputPanel = ic_ -> inputPanel();
	inputPanel.reset();
	if (!BoshiamyData::get_typed_word(buffer_.userInput()).empty() && !buffer_.empty()) {
		int idx = 0;
		FCITX_INFO() << "entered getting candidate list";
		if (inputPanel.candidateList() != nullptr) {
			idx = inputPanel.candidateList() -> cursorIndex();
		}
		FCITX_INFO() << "finished getting cursor index";
		inputPanel.setCandidateList(std::make_unique<ShamiCandidateList>(engine_, ic_, buffer_.userInput()));
		FCITX_INFO() << "finished candidate list";
		// inputPanel.candidateList() -> toCursorModifiable() -> setCursorIndex(idx);
	}
	fcitx::Text preedit(buffer_.userInput());
	if (ic_ -> capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
		preedit = fcitx::Text(buffer_.userInput(), 
				fcitx::TextFormatFlag::HighLight);
	}
	inputPanel.setPreedit(preedit);
	ic_ -> updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
	ic_ -> updatePreedit();
	FCITX_INFO() << "Finish updating UI";
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
