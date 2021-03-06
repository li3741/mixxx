#include <QList>
#include <QDir>
#include <QToolTip>
#include <QDoubleSpinBox>
#include <QWidget>
#include <QLocale>
#include <QDesktopWidget>

#include "preferences/dialog/dlgprefinterface.h"
#include "preferences/usersettings.h"
#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "skin/skinloader.h"
#include "skin/legacyskinparser.h"
#include "control/controlobject.h"
#include "mixxx.h"
#include "util/screensaver.h"
#include "defs_urls.h"
#include "util/autohidpi.h"

DlgPrefInterface::DlgPrefInterface(QWidget * parent, MixxxMainWindow * mixxx,
                                 SkinLoader* pSkinLoader,
                                 UserSettingsPointer pConfig)
        :  DlgPreferencePage(parent),
           m_pConfig(pConfig),
           m_mixxx(mixxx),
           m_pSkinLoader(pSkinLoader),
           m_dScaleFactorAuto(1.0),
           m_bUseAutoScaleFactor(false),
           m_dScaleFactor(1.0),
           m_bStartWithFullScreen(false),
           m_bRebootMixxxView(false) {
    setupUi(this);

    //
    // Locale setting
    //

    // Iterate through the available locales and add them to the combobox
    // Borrowed following snippet from http://qt-project.org/wiki/How_to_create_a_multi_language_application
    QString translationsFolder = m_pConfig->getResourcePath() + "translations/";
    QString currentLocale = pConfig->getValueString(ConfigKey("[Config]", "Locale"));

    QDir translationsDir(translationsFolder);
    QStringList fileNames = translationsDir.entryList(QStringList("mixxx_*.qm"));
    fileNames.push_back("mixxx_en_US.qm"); // add source language as a fake value

    bool indexFlag = false; // it'll indicate if the selected index changed.
    for (int i = 0; i < fileNames.size(); ++i) {
        // Extract locale from filename
        QString locale = fileNames[i];
        locale.truncate(locale.lastIndexOf('.'));
        locale.remove(0, locale.indexOf('_') + 1);
        QLocale qlocale = QLocale(locale);

        QString lang = QLocale::languageToString(qlocale.language());
        QString country = QLocale::countryToString(qlocale.country());
        if (lang == "C") { // Ugly hack to remove the non-resolving locales
            continue;
        }
        lang = QString("%1 (%2)").arg(lang).arg(country);
        ComboBoxLocale->addItem(lang, locale); // locale as userdata (for storing to config)
        if (locale == currentLocale) { // Set the currently selected locale
            ComboBoxLocale->setCurrentIndex(ComboBoxLocale->count() - 1);
            indexFlag = true;
        }
    }
    ComboBoxLocale->model()->sort(0); // Sort languages list

    ComboBoxLocale->insertItem(0, "System", ""); // System default locale - insert at the top
    if (!indexFlag) { // if selectedIndex didn't change - select system default
        ComboBoxLocale->setCurrentIndex(0);
    }
    connect(ComboBoxLocale, SIGNAL(activated(int)),
            this, SLOT(slotSetLocale(int)));

    //
    // Skin configurations
    //
    QString warningString = "<img src=\":/images/preferences/ic_preferences_warning.png\") width=16 height=16 />"
        + tr("The minimum size of the selected skin is bigger than your screen resolution.");
    warningLabel->setText(warningString);

    ComboBoxSkinconf->clear();

    QList<QDir> skinSearchPaths = m_pSkinLoader->getSkinSearchPaths();
    QList<QFileInfo> skins;
    for (QDir& dir : skinSearchPaths) {
        dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
        skins.append(dir.entryInfoList());
    }

    QString configuredSkinPath = m_pSkinLoader->getConfiguredSkinPath();
    QIcon sizeWarningIcon(":/images/preferences/ic_preferences_warning.png");
    int index = 0;
    for (const QFileInfo& skinInfo : skins) {
        bool size_ok = checkSkinResolution(skinInfo.absoluteFilePath());
        if (size_ok) {
            ComboBoxSkinconf->insertItem(index, skinInfo.fileName());
        } else {
            ComboBoxSkinconf->insertItem(index, sizeWarningIcon, skinInfo.fileName());
        }

        if (skinInfo.absoluteFilePath() == configuredSkinPath) {
            m_skin = skinInfo.fileName();
            ComboBoxSkinconf->setCurrentIndex(index);
            if (size_ok) {
                warningLabel->hide();
            } else {
                warningLabel->show();
            }
        }
        index++;
    }

    connect(ComboBoxSkinconf, SIGNAL(activated(int)), this, SLOT(slotSetSkin(int)));
    connect(ComboBoxSchemeconf, SIGNAL(activated(int)), this, SLOT(slotSetScheme(int)));

    slotUpdateSchemes();


#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    AutoHiDpi autoHiDpi;
    m_dScaleFactorAuto = autoHiDpi.getScaleFactor();
    double scaleFactor = m_dScaleFactorAuto;
    if (scaleFactor > 0) {
        // we got a valid auto scale factor
        bool scaleFactorAuto = m_pConfig->getValue(
                ConfigKey("[Config]", "ScaleFactorAuto"), true);
        checkBoxScaleFactorAuto->setChecked(scaleFactorAuto);
        if (scaleFactorAuto) {
            comboBoxScaleFactor->setEnabled(false);
            m_pConfig->setValue(
                    ConfigKey("[Config]", "ScaleFactor"), m_dScaleFactorAuto);
        } else {
            scaleFactor = m_pConfig->getValue(
                        ConfigKey("[Config]", "ScaleFactor"), 1.0);
        }
    } else {
        checkBoxScaleFactorAuto->setEnabled(false);
        scaleFactor = m_pConfig->getValue(
                    ConfigKey("[Config]", "ScaleFactor"), 1.0);
    }
    connect(checkBoxScaleFactorAuto, SIGNAL(toggled(bool)),
            this, SLOT(slotSetScaleFactorAuto(bool)));

    //: Entry of the HiDPI scale combo box. %1 is the scale factor in percent
    comboBoxScaleFactor->addItem(QString(tr("%1 %")).arg(50), 0.5);
    comboBoxScaleFactor->addItem(QString(tr("%1 %")).arg(100), 1);
    comboBoxScaleFactor->addItem(QString(tr("%1 %")).arg(200), 2);
    comboBoxScaleFactor->addItem(QString(tr("%1 %")).arg(300), 3);
    comboBoxScaleFactor->addItem(QString(tr("%1 %")).arg(400), 4);
    int i;
    for (i = 0; i < comboBoxScaleFactor->count(); ++i) {
        if (scaleFactor == comboBoxScaleFactor->itemData(i)) {
            comboBoxScaleFactor->setCurrentIndex(i);
            break;
        }
    }
    if (i == comboBoxScaleFactor->count()) {
        // no default scale, add custom scale
        comboBoxScaleFactor->addItem(
                QString(tr("%1 % (Experimental)")).arg(scaleFactor * 100), scaleFactor);
        comboBoxScaleFactor->setCurrentIndex(i);
    }
    connect(comboBoxScaleFactor, SIGNAL(activated(int)),
            this, SLOT(slotSetScaleFactor(int)));
#else
    checkBoxScaleFactorAuto->hide();
    comboBoxScaleFactor->hide();
    labelScaleFactor->hide();
#endif


    //
    // Start in fullscreen mode
    //
    checkBoxStartFullScreen->setChecked(m_pConfig->getValueString(
                    ConfigKey("[Config]", "StartInFullscreen")).toInt()==1);

    //
    // Screensaver mode
    //
    comboBoxScreensaver->clear();
    comboBoxScreensaver->addItem(tr("Allow screensaver to run"), 
        static_cast<int>(mixxx::ScreenSaverPreference::PREVENT_OFF));
    comboBoxScreensaver->addItem(tr("Prevent screensaver from running"), 
        static_cast<int>(mixxx::ScreenSaverPreference::PREVENT_ON));
    comboBoxScreensaver->addItem(tr("Prevent screensaver while playing"), 
        static_cast<int>(mixxx::ScreenSaverPreference::PREVENT_ON_PLAY));

    int inhibitsettings = static_cast<int>(mixxx->getInhibitScreensaver());
    comboBoxScreensaver->setCurrentIndex(comboBoxScreensaver->findData(inhibitsettings));

    //
    // Tooltip configuration
    //

    // Initialize checkboxes to match config
    loadTooltipPreferenceFromConfig();
    slotSetTooltips();  // Update disabled status of "only library" checkbox
    connect(buttonGroupTooltips, SIGNAL(buttonClicked(QAbstractButton*)),
            this, SLOT(slotSetTooltips()));

    slotUpdate();
}

DlgPrefInterface::~DlgPrefInterface() {
}

void DlgPrefInterface::slotUpdateSchemes() {
    // Since this involves opening a file we won't do this as part of regular slotUpdate
    QList<QString> schlist = LegacySkinParser::getSchemeList(
                m_pSkinLoader->getSkinPath(m_skin));

    ComboBoxSchemeconf->clear();

    if (schlist.size() == 0) {
        ComboBoxSchemeconf->setEnabled(false);
        ComboBoxSchemeconf->addItem(tr("This skin does not support color schemes", 0));
        ComboBoxSchemeconf->setCurrentIndex(0);
    } else {
        ComboBoxSchemeconf->setEnabled(true);
        QString selectedScheme = m_pConfig->getValueString(ConfigKey("[Config]", "Scheme"));
        for (int i = 0; i < schlist.size(); i++) {
            ComboBoxSchemeconf->addItem(schlist[i]);

            if (schlist[i] == selectedScheme) {
                ComboBoxSchemeconf->setCurrentIndex(i);
            }
        }
    }
}

void DlgPrefInterface::slotUpdate() {
    m_skinOnUpdate = m_pConfig->getValueString(ConfigKey("[Config]", "ResizableSkin"));
    ComboBoxSkinconf->setCurrentIndex(ComboBoxSkinconf->findText(m_skinOnUpdate));
    slotUpdateSchemes();
    m_bRebootMixxxView = false;

    m_localeOnUpdate = m_pConfig->getValueString(ConfigKey("[Config]", "Locale"));
    ComboBoxLocale->setCurrentIndex(ComboBoxLocale->findData(m_localeOnUpdate));

    checkBoxScaleFactorAuto->setChecked(m_pConfig->getValue(
            ConfigKey("[Config]", "ScaleFactorAuto"), m_bUseAutoScaleFactor));

    comboBoxScaleFactor->setCurrentIndex(comboBoxScaleFactor->findData(
            m_pConfig->getValue(
                    ConfigKey("[Config]", "ScaleFactor"), m_dScaleFactorAuto)));

    checkBoxStartFullScreen->setChecked(m_pConfig->getValue(
            ConfigKey("[Config]", "StartInFullscreen"), m_bStartWithFullScreen));

    loadTooltipPreferenceFromConfig();

    int inhibitsettings = static_cast<int>(m_mixxx->getInhibitScreensaver());
    comboBoxScreensaver->setCurrentIndex(comboBoxScreensaver->findData(inhibitsettings));
}

void DlgPrefInterface::slotResetToDefaults() {
    int index = ComboBoxSkinconf->findText(m_pSkinLoader->getDefaultSkinName());
    ComboBoxSkinconf->setCurrentIndex(index);
    slotSetSkin(index);

    // Use System locale
    ComboBoxLocale->setCurrentIndex(0);

    // Default to normal size widgets
    comboBoxScaleFactor->setCurrentIndex(1); // 100 %
    if (m_dScaleFactorAuto > 0) {
        checkBoxScaleFactorAuto->setChecked(true);
    }

    // Don't start in full screen.
    checkBoxStartFullScreen->setChecked(false);

    // Inhibit the screensaver
    comboBoxScreensaver->setCurrentIndex(comboBoxScreensaver->findData(
        static_cast<int>(mixxx::ScreenSaverPreference::PREVENT_ON)));

    // Tooltips on everywhere.
    radioButtonTooltipsLibraryAndSkin->setChecked(true);
}

void DlgPrefInterface::slotSetLocale(int pos) {
    m_locale = ComboBoxLocale->itemData(pos).toString();
}

void DlgPrefInterface::slotSetScaleFactor(int index) {
    double newScaleFactor = comboBoxScaleFactor->itemData(index).toDouble();
    if (m_dScaleFactor != newScaleFactor) {
        m_dScaleFactor = newScaleFactor;
        m_bRebootMixxxView = true;
    }
}

void DlgPrefInterface::slotSetScaleFactorAuto(bool newValue) {
    if (newValue) {
        if (!m_bUseAutoScaleFactor) {
            m_bRebootMixxxView = true;
        }
    } else {
        slotSetScaleFactor(comboBoxScaleFactor->currentIndex());
    }

    m_bUseAutoScaleFactor = newValue;
    comboBoxScaleFactor->setEnabled(!newValue);
}

void DlgPrefInterface::slotSetTooltips() {
    m_tooltipMode = mixxx::TooltipsPreference::TOOLTIPS_ON;
    if (radioButtonTooltipsOff->isChecked()) {
        m_tooltipMode = mixxx::TooltipsPreference::TOOLTIPS_OFF;
    } else if (radioButtonTooltipsLibrary->isChecked()) {
        m_tooltipMode = mixxx::TooltipsPreference::TOOLTIPS_ONLY_IN_LIBRARY;
    }
}

void DlgPrefInterface::notifyRebootNecessary() {
    // make the fact that you have to restart mixxx more obvious
    QMessageBox::information(
        this, tr("Information"),
        tr("Mixxx must be restarted before the new locale setting will take effect."));
}

void DlgPrefInterface::slotSetScheme(int) {
    QString newScheme = ComboBoxSchemeconf->currentText();
    if (m_colorScheme != newScheme) {
        m_colorScheme = newScheme;
        m_bRebootMixxxView = true;
    }
}

void DlgPrefInterface::slotSetSkin(int) {
    QString newSkin = ComboBoxSkinconf->currentText();
    if (newSkin != m_skin) {
        m_skin = newSkin;
        m_bRebootMixxxView = newSkin != m_skinOnUpdate;
        checkSkinResolution(ComboBoxSkinconf->currentText())
            ? warningLabel->hide() : warningLabel->show();
        slotUpdateSchemes();
    }
}

void DlgPrefInterface::slotApply() {
    m_pConfig->set(ConfigKey("[Config]", "ResizableSkin"), m_skin);
    m_pConfig->set(ConfigKey("[Config]", "Scheme"), m_colorScheme);

    m_pConfig->set(ConfigKey("[Config]", "Locale"), m_locale);

    m_pConfig->setValue(
            ConfigKey("[Config]", "ScaleFactorAuto"), m_bUseAutoScaleFactor);
    if (m_bUseAutoScaleFactor) {
        m_pConfig->setValue(
                ConfigKey("[Config]", "ScaleFactor"), m_dScaleFactorAuto);
    } else {
        m_pConfig->setValue(ConfigKey("[Config]", "ScaleFactor"), m_dScaleFactor);
    }

    m_pConfig->set(ConfigKey("[Config]", "StartInFullscreen"),
            ConfigValue(checkBoxStartFullScreen->isChecked()));

    m_mixxx->setToolTipsCfg(m_tooltipMode);

    // screensaver mode update
    int screensaverComboBoxState = comboBoxScreensaver->itemData(
            comboBoxScreensaver->currentIndex()).toInt();
    int screensaverConfiguredState = static_cast<int>(m_mixxx->getInhibitScreensaver());
    if (screensaverComboBoxState != screensaverConfiguredState) {
        m_mixxx->setInhibitScreensaver(
                static_cast<mixxx::ScreenSaverPreference>(screensaverComboBoxState));
    }

    if (m_locale != m_localeOnUpdate) {
        notifyRebootNecessary();
        // hack to prevent showing the notification when pressing "Okay" after "Apply"
        m_localeOnUpdate = m_locale;
    }

    if (m_bRebootMixxxView) {
        m_mixxx->rebootMixxxView();
        // Allow switching skins multiple times without closing the dialog
        m_skinOnUpdate = m_skin;
    }
    m_bRebootMixxxView = false;
}

//Returns TRUE if skin fits to screen resolution, FALSE otherwise
bool DlgPrefInterface::checkSkinResolution(QString skin)
{
    int screenWidth = QApplication::desktop()->width();
    int screenHeight = QApplication::desktop()->height();

    const QRegExp min_size_regex("<MinimumSize>(\\d+), *(\\d+)<");
    QFile skinfile(skin + "/skin.xml");
    if (skinfile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&skinfile);
        bool found_size = false;
        while (!in.atEnd()) {
            if (min_size_regex.indexIn(in.readLine()) != -1) {
                found_size = true;
                break;
            }
        }
        if (found_size) {
            return !(min_size_regex.cap(1).toInt() > screenWidth ||
                     min_size_regex.cap(2).toInt() > screenHeight);
        }
    }

    // If regex failed, fall back to skin name parsing.
    QString skinName = skin.left(skin.indexOf(QRegExp("\\d")));
    QString resName = skin.right(skin.count()-skinName.count());
    QString res = resName.left(resName.lastIndexOf(QRegExp("\\d"))+1);
    QString skinWidth = res.left(res.indexOf("x"));
    QString skinHeight = res.right(res.count()-skinWidth.count()-1);
    return !(skinWidth.toInt() > screenWidth || skinHeight.toInt() > screenHeight);
}

void DlgPrefInterface::loadTooltipPreferenceFromConfig() {
    mixxx::TooltipsPreference configTooltips = m_mixxx->getToolTipsCfg();
    switch (configTooltips) {
        case mixxx::TooltipsPreference::TOOLTIPS_OFF:
            radioButtonTooltipsOff->setChecked(true);
            break;
        case mixxx::TooltipsPreference::TOOLTIPS_ON:
            radioButtonTooltipsLibraryAndSkin->setChecked(true);
            break;
        case mixxx::TooltipsPreference::TOOLTIPS_ONLY_IN_LIBRARY:
            radioButtonTooltipsLibrary->setChecked(true);
            break;
    }
    m_tooltipMode = configTooltips;
}
