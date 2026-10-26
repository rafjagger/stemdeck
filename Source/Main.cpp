/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MainComponent.h"
#include "OneDeckComponent.h"
#include "DeckInstance.h"
#include "LaunchOptions.h"

#include <iostream>

//==============================================================================
class djApplication_LaurentKleinApplication  : public juce::JUCEApplication
{
public:
    //==============================================================================
    djApplication_LaurentKleinApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        const auto parsed = parseLaunch (toStdStrings (getCommandLineParameterArray()));

        if (const auto* why = std::get_if<std::string> (&parsed))
        {
            std::cerr << "StemDeck: " << *why << std::endl;
            setApplicationReturnValue (2);
            quit();
            return;
        }

        const auto& options = std::get<LaunchOptions> (parsed);

        // -bformat: one deck per instance (R10); without it the two-deck app
        // as it always was. Each keeps its window's place under its own key.
        if (options.oneDeck)
        {
            const DeckInstance instance { options.deck };
            auto content = std::make_unique<OneDeckComponent> (instance);
            auto& settings = content->settings();
            mainWindow.reset (new MainWindow (juce::String (instance.title()), std::move (content), settings,
                                              "windowState." + juce::String::charToString (instance.letter)));
        }
        else
        {
            auto content = std::make_unique<MainComponent>();
            auto& settings = content->settings();
            mainWindow.reset (new MainWindow (getApplicationName(), std::move (content), settings, "windowState"));
        }

        // The rig's service starts it with --fullscreen: the i3 rule alone
        // missed it, catching one of the windows JUCE builds and drops again
        // while StemDeck comes up (2026-09-30).
        if (options.fullscreen)
            mainWindow->setFullScreen (true);
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, std::unique_ptr<juce::Component> content, juce::PropertiesFile& contentSettings,
                    juce::String windowStateKey)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons),
              settings (contentSettings), stateKey (windowStateKey)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content.release(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            // Where it was last time (StemDeck.settings).
            if (const auto state = settings.getValue (stateKey); state.isNotEmpty())
                restoreWindowStateFromString (state);
           #endif

            setVisible (true);
        }

        ~MainWindow() override
        {
            // The settings belong to the content, deleted after this body.
            settings.setValue (stateKey, getWindowStateAsString());
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        juce::PropertiesFile& settings;
        const juce::String stateKey;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    static std::vector<std::string> toStdStrings (const juce::StringArray& words)
    {
        std::vector<std::string> out;
        for (const auto& word : words)
            out.push_back (word.toStdString());
        return out;
    }

    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (djApplication_LaurentKleinApplication)
