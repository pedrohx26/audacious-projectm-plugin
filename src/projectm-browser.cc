/*
 * ProjectM Preset Browser Plugin for Audacious
 * Copyright 2026
 *
 * A General Plugin that shows a dockable preset browser panel.
 * Communicates with the projectm-vis Visualization Plugin via the
 * Audacious hook "projectm-vis set-preset".
 *
 * License: GPLv2+
 */

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

static constexpr const char * PM_CFG_SECTION      = "projectm-vis";
static constexpr const char * PM_HOOK_SET_PRESET   = "projectm-vis set-preset";

/* ─── Preset Browser Widget ────────────────────────────────────────── */

class PresetBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PresetBrowserWidget (QWidget * parent = nullptr);

    void refresh ();

private slots:
    void on_search_changed (const QString & text);
    void on_item_activated (QListWidgetItem * item);
    void on_load_clicked ();
    void on_refresh_clicked ();
    void on_featured_clicked ();

private:
    QLineEdit  * m_search;
    QListWidget * m_list;
    QLabel     * m_count;

    QStringList            m_all;     /* sorted preset names     */
    QMap<QString, QString> m_map;     /* display name → full path */

    void populate ();
    void rebuild  ();
    void fire_preset (const QString & path);

    QStringList preset_dirs () const;
};

/* ------------------------------------------------------------------ */

PresetBrowserWidget::PresetBrowserWidget (QWidget * parent)
    : QWidget (parent)
{
    auto * layout = new QVBoxLayout (this);
    layout->setContentsMargins (4, 4, 4, 4);
    layout->setSpacing (4);

    /* Search bar */
    m_search = new QLineEdit ();
    m_search->setPlaceholderText (tr ("Search presets…"));
    m_search->setClearButtonEnabled (true);
    layout->addWidget (m_search);

    /* Preset list */
    m_list = new QListWidget ();
    m_list->setAlternatingRowColors (true);
    layout->addWidget (m_list, 1);

    /* Count */
    m_count = new QLabel ();
    layout->addWidget (m_count);

    /* Buttons */
    auto * btns = new QHBoxLayout ();
    auto * featured_btn = new QPushButton (tr ("⭐ Featured"));
    auto * refresh_btn  = new QPushButton (tr ("↺ Refresh"));
    auto * load_btn     = new QPushButton (tr ("▶ Load"));
    featured_btn->setToolTip (tr ("Show featured presets only"));
    refresh_btn->setToolTip  (tr ("Rescan preset directories"));
    load_btn->setToolTip     (tr ("Load selected preset (double-click also works)"));
    btns->addWidget (featured_btn);
    btns->addWidget (refresh_btn);
    btns->addStretch ();
    btns->addWidget (load_btn);
    layout->addLayout (btns);

    connect (m_search,     &QLineEdit::textChanged,        this, &PresetBrowserWidget::on_search_changed);
    connect (m_list,       &QListWidget::itemActivated,    this, &PresetBrowserWidget::on_item_activated);
    connect (m_list,       &QListWidget::itemDoubleClicked,this, &PresetBrowserWidget::on_item_activated);
    connect (load_btn,     &QPushButton::clicked,          this, &PresetBrowserWidget::on_load_clicked);
    connect (refresh_btn,  &QPushButton::clicked,          this, &PresetBrowserWidget::on_refresh_clicked);
    connect (featured_btn, &QPushButton::clicked,          this, &PresetBrowserWidget::on_featured_clicked);

    populate ();
}

QStringList PresetBrowserWidget::preset_dirs () const
{
    QStringList dirs;

    /* User-configured path (shared with the vis plugin) */
    String cfg = aud_get_str (PM_CFG_SECTION, "preset_path");
    if (cfg && cfg[0])
    {
        QString p = QString::fromUtf8 ((const char *) cfg);
        if (QDir (p).exists () && ! dirs.contains (p))
            dirs << p;
    }

    /* Standard install locations */
    const QStringList standard = {
        QDir::homePath () + "/.local/share/projectM/presets",
        "/usr/share/projectM/presets",
        "/usr/local/share/projectM/presets",
        QDir::homePath () + "/.projectM/presets",
    };
    for (const auto & p : standard)
        if (QDir (p).exists () && ! dirs.contains (p))
            dirs << p;

    return dirs;
}

void PresetBrowserWidget::populate ()
{
    m_map.clear ();
    m_all.clear ();

    for (const auto & dir_path : preset_dirs ())
    {
        QDir dir (dir_path);
        const QStringList files = dir.entryList ({QStringLiteral("*.milk"),
                                                  QStringLiteral("*.prjm")},
                                                 QDir::Files,
                                                 QDir::Name);
        for (const auto & f : files)
        {
            if (! m_map.contains (f))
            {
                m_map[f] = dir.filePath (f);
                m_all << f;
            }
        }
    }

    std::sort (m_all.begin (), m_all.end ());
    rebuild ();
}

void PresetBrowserWidget::rebuild ()
{
    const QString q = m_search->text ().toLower ();
    m_list->clear ();

    for (const auto & name : m_all)
        if (q.isEmpty () || name.toLower ().contains (q))
            m_list->addItem (name);

    m_count->setText (tr ("%1 / %2 presets").arg (m_list->count ()).arg (m_all.size ()));
}

void PresetBrowserWidget::refresh ()
{
    populate ();
}

void PresetBrowserWidget::fire_preset (const QString & path)
{
    /* Pass path to the vis plugin via Audacious hook.
     * hook_call() is synchronous, so the QByteArray stays alive. */
    QByteArray ba = path.toUtf8 ();
    hook_call (PM_HOOK_SET_PRESET, (void *) ba.constData ());
}

/* slots */
void PresetBrowserWidget::on_search_changed (const QString &) { rebuild (); }
void PresetBrowserWidget::on_refresh_clicked ()               { populate (); }

void PresetBrowserWidget::on_item_activated (QListWidgetItem * item)
{
    const QString & name = item->text ();
    if (m_map.contains (name))
        fire_preset (m_map[name]);
}

void PresetBrowserWidget::on_load_clicked ()
{
    auto * item = m_list->currentItem ();
    if (item)
        on_item_activated (item);
}

void PresetBrowserWidget::on_featured_clicked ()
{
    const QString featured = QDir::homePath () + "/.local/share/projectM/presets/featured";
    if (! QDir (featured).exists ())
        return;

    /* Show only the featured presets */
    m_map.clear ();
    m_all.clear ();

    QDir dir (featured);
    const QStringList files = dir.entryList ({QStringLiteral("*.milk"),
                                              QStringLiteral("*.prjm")},
                                             QDir::Files,
                                             QDir::Name);
    for (const auto & f : files)
    {
        m_map[f] = dir.filePath (f);
        m_all << f;
    }

    m_search->clear ();
    rebuild ();
}


/* ─── General Plugin ────────────────────────────────────────────── */

class PresetBrowserPlugin : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("ProjectM Preset Browser"),
        PACKAGE,
        nullptr,           /* no about text */
        nullptr,           /* no preferences */
        PluginQtOnly
    };

    constexpr PresetBrowserPlugin () : GeneralPlugin (info, false) {}

    void * get_qt_widget () override;
};

void * PresetBrowserPlugin::get_qt_widget ()
{
    return new PresetBrowserWidget ();
}

extern "C" PresetBrowserPlugin aud_plugin_instance;
PresetBrowserPlugin aud_plugin_instance;

#include "projectm-browser.moc"
