#ifndef _FCITX5_SHAMI_SHAMI_H_
#define _FCITX5_SHAMI_SHAMI_H_

#include <fcitx/inputmethodengine.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx-utils/inputbuffer.h>

class ShamiEngine;

class ShamiState: public fcitx::InputContextProperty {
public:
	ShamiState(ShamiEngine* engine, fcitx::InputContext* ic): engine_(engine), ic_(ic) {}
	bool handleCandidateKeyEvent(fcitx::KeyEvent &event);
	bool handleNormalKeyEvent(fcitx::KeyEvent &event);
	void commitBuffer();
	void keyEvent(fcitx::KeyEvent &keyEvent);
	void updateUI();
	void reset() {
		buffer_.clear();
		updateUI();
	}
private:
	ShamiEngine* engine_;
	fcitx::InputContext* ic_;
	fcitx::InputBuffer buffer_{{fcitx::InputBufferOption::AsciiOnly, fcitx::InputBufferOption::FixedCursor}};
};

class ShamiEngine : public fcitx::InputMethodEngineV2 {
public:
	ShamiEngine(fcitx::Instance* instance);
    void keyEvent(const fcitx::InputMethodEntry & entry, 
			fcitx::KeyEvent & keyEvent) override;
	void reset(const fcitx::InputMethodEntry &,
			fcitx::InputContextEvent &event) override;
	auto factory() const { return &factory_; }
    auto instance() const { return instance_; }
private:
    fcitx::Instance *instance_;
    fcitx::FactoryFor<ShamiState> factory_;
};

class ShamiEngineFactory : public fcitx::AddonFactory {
    fcitx::AddonInstance * create(fcitx::AddonManager * manager) override {
        FCITX_UNUSED(manager);
        return new ShamiEngine(manager -> instance());
    }
};

#endif // _FCITX5_SHAMI_SHAMI_H_
