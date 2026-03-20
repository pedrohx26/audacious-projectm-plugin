/*
 * ProjectM Visualization Plugin for Audacious
 * Copyright 2026 – Milkdrop preset visualizer using libprojectM 4.x
 *
 * License: GPLv2+
 *
 * Features:
 *   - Milkdrop preset rendering via libprojectM 4.x + OpenGL
 *   - Audacious menu integration (Visualization menu entries)
 *   - Preset browser with search/filter
 *   - Preset variable editor – parse & edit .milk INI-style variables live
 *   - Keyboard shortcuts for navigation
 *   - Settings persistence via Audacious plugin preferences
 */

#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudcore/hook.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/menu.h>

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QFileDialog>
#include <QListWidget>
#include <QSplitter>
#include <QComboBox>
#include <QSlider>
#include <QGroupBox>
#include <QScrollArea>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QDialog>
#include <QMenu>
#include <QAction>
#include <QMenuBar>
#include <QMainWindow>
#include <QRegularExpression>
#include <QMap>
#include <QFile>
#include <QTextStream>

#include <projectM-4/projectM.h>
#include <projectM-4/playlist.h>

#include <cmath>
#include <cstring>
#include <vector>
#include <string>

/* ─── forward declarations ─────────────────────────────────────────── */
class ProjectMWidget;
class ProjectMContainer;
class PresetBrowser;
class PresetVarEditor;

static ProjectMContainer * s_container = nullptr;
static PresetBrowser * s_browser = nullptr;
static constexpr const char * PM_CFG_SECTION = "projectm-vis";

static const char * const pm_defaults[] = {
    "preset_path", "",
    "fps", "60",
    "beat_sensitivity", "100",
    "hard_cut", "TRUE",
    "hard_cut_duration", "20.0",
    "soft_cut_duration", "10.0",
    nullptr
};

static void pm_settings_changed ();

static QStringList pm_texture_search_paths ()
{
    return {
        QDir::homePath () + "/.local/share/projectM/textures",
        QDir::homePath () + "/.local/share/projectM/presets/textures",
        "/usr/share/projectM/textures",
        "/usr/share/projectM/presets/textures",
        "/usr/local/share/projectM/textures",
        "/usr/local/share/projectM/presets/textures"
    };
}

/* ─── Milkdrop preset file parser ──────────────────────────────────── */

struct MilkdropVar {
    QString key;
    QString value;
    QString section;   // "general", "per_frame_init", "per_frame", "per_pixel", "warp", "comp"
    int     line_num;
};

class MilkdropPresetFile {
public:
    bool load (const QString & path);
    bool save (const QString & path) const;
    bool save () const { return save (m_path); }

    QString path () const { return m_path; }
    QList<MilkdropVar> variables () const { return m_vars; }
    QStringList raw_lines () const { return m_lines; }

    /* general variables (zoom, rot, warp, decay, …) */
    QMap<QString, QString> general_vars () const;

    /* equation blocks */
    QString per_frame_init () const { return m_per_frame_init; }
    QString per_frame () const     { return m_per_frame; }
    QString per_pixel () const     { return m_per_pixel; }
    QString warp_shader () const   { return m_warp; }
    QString comp_shader () const   { return m_comp; }

    void set_general_var (const QString & key, const QString & value);
    void set_per_frame_init (const QString & code);
    void set_per_frame (const QString & code);
    void set_per_pixel (const QString & code);

private:
    QString m_path;
    QStringList m_lines;
    QList<MilkdropVar> m_vars;

    QString m_per_frame_init;
    QString m_per_frame;
    QString m_per_pixel;
    QString m_warp;
    QString m_comp;

    void parse ();
};

bool MilkdropPresetFile::load (const QString & path)
{
    QFile f (path);
    if (! f.open (QIODevice::ReadOnly | QIODevice::Text))
        return false;

    m_path = path;
    m_lines.clear ();
    m_vars.clear ();

    QTextStream in (&f);
    while (! in.atEnd ())
        m_lines.append (in.readLine ());
    f.close ();

    parse ();
    return true;
}

void MilkdropPresetFile::parse ()
{
    m_vars.clear ();
    m_per_frame_init.clear ();
    m_per_frame.clear ();
    m_per_pixel.clear ();
    m_warp.clear ();
    m_comp.clear ();

    /*
     * Milkdrop .milk files are INI-like:
     *   [PRESET HEADER]
     *   key=value
     *   ...
     *   per_frame_init_1=...
     *   per_frame_1=...
     *   per_pixel_1=...
     *   ...
     *   [preset00]  (sometimes)
     *
     * Numbered equations: per_frame_init_N, per_frame_N, per_pixel_N
     * Shader blocks: warp_N, comp_N
     */

    QMap<int, QString> pfi_map, pf_map, pp_map, warp_map, comp_map;

    for (int i = 0; i < m_lines.size (); i++)
    {
        const QString & line = m_lines[i];
        QString trimmed = line.trimmed ();

        if (trimmed.isEmpty () || trimmed.startsWith ('[') || trimmed.startsWith ("//"))
            continue;

        int eq = trimmed.indexOf ('=');
        if (eq < 1)
            continue;

        QString key = trimmed.left (eq).trimmed ();
        QString val = trimmed.mid (eq + 1).trimmed ();

        /* Classify */
        static QRegularExpression re_pfi ("^per_frame_init_(\\d+)$");
        static QRegularExpression re_pf  ("^per_frame_(\\d+)$");
        static QRegularExpression re_pp  ("^per_pixel_(\\d+)$");
        static QRegularExpression re_warp ("^warp_(\\d+)$");
        static QRegularExpression re_comp ("^comp_(\\d+)$");

        auto m = re_pfi.match (key);
        if (m.hasMatch ()) { pfi_map[m.captured (1).toInt ()] = val; continue; }
        m = re_pf.match (key);
        if (m.hasMatch ()) { pf_map[m.captured (1).toInt ()] = val; continue; }
        m = re_pp.match (key);
        if (m.hasMatch ()) { pp_map[m.captured (1).toInt ()] = val; continue; }
        m = re_warp.match (key);
        if (m.hasMatch ()) { warp_map[m.captured (1).toInt ()] = val; continue; }
        m = re_comp.match (key);
        if (m.hasMatch ()) { comp_map[m.captured (1).toInt ()] = val; continue; }

        /* General variable */
        MilkdropVar var;
        var.key = key;
        var.value = val;
        var.section = "general";
        var.line_num = i;
        m_vars.append (var);
    }

    /* Reconstruct equation blocks from numbered lines */
    auto join_map = [](const QMap<int,QString> & m) -> QString {
        QStringList parts;
        for (auto it = m.constBegin (); it != m.constEnd (); ++it)
            parts.append (it.value ());
        return parts.join ("\n");
    };

    m_per_frame_init = join_map (pfi_map);
    m_per_frame = join_map (pf_map);
    m_per_pixel = join_map (pp_map);
    m_warp = join_map (warp_map);
    m_comp = join_map (comp_map);
}

bool MilkdropPresetFile::save (const QString & path) const
{
    QFile f (path);
    if (! f.open (QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out (&f);
    for (const auto & line : m_lines)
        out << line << "\n";
    f.close ();
    return true;
}

QMap<QString, QString> MilkdropPresetFile::general_vars () const
{
    QMap<QString, QString> result;
    for (const auto & v : m_vars)
        if (v.section == "general")
            result[v.key] = v.value;
    return result;
}

void MilkdropPresetFile::set_general_var (const QString & key, const QString & value)
{
    /* Update in-memory lines */
    for (int i = 0; i < m_lines.size (); i++)
    {
        QString trimmed = m_lines[i].trimmed ();
        int eq = trimmed.indexOf ('=');
        if (eq > 0 && trimmed.left (eq).trimmed () == key)
        {
            m_lines[i] = key + "=" + value;
            /* Also update m_vars */
            for (auto & v : m_vars)
                if (v.key == key) { v.value = value; break; }
            return;
        }
    }
    /* Not found – append before equation blocks */
    for (int i = 0; i < m_lines.size (); i++)
    {
        if (m_lines[i].trimmed ().startsWith ("per_frame"))
        {
            m_lines.insert (i, key + "=" + value);
            MilkdropVar var;
            var.key = key;
            var.value = value;
            var.section = "general";
            var.line_num = i;
            m_vars.append (var);
            return;
        }
    }
    m_lines.append (key + "=" + value);
}

void MilkdropPresetFile::set_per_frame_init (const QString & code)
{
    m_per_frame_init = code;
    /* Remove old per_frame_init_N lines */
    m_lines.erase (std::remove_if (m_lines.begin (), m_lines.end (),
        [](const QString & l) {
            return l.trimmed ().contains (QRegularExpression ("^per_frame_init_\\d+="));
        }), m_lines.end ());
    /* Append new */
    QStringList parts = code.split ("\n");
    for (int i = 0; i < parts.size (); i++)
        m_lines.append (QString ("per_frame_init_%1=%2").arg (i + 1).arg (parts[i]));
}

void MilkdropPresetFile::set_per_frame (const QString & code)
{
    m_per_frame = code;
    m_lines.erase (std::remove_if (m_lines.begin (), m_lines.end (),
        [](const QString & l) {
            return l.trimmed ().contains (QRegularExpression ("^per_frame_\\d+="))
                && ! l.trimmed ().contains (QRegularExpression ("^per_frame_init_"));
        }), m_lines.end ());
    QStringList parts = code.split ("\n");
    for (int i = 0; i < parts.size (); i++)
        m_lines.append (QString ("per_frame_%1=%2").arg (i + 1).arg (parts[i]));
}

void MilkdropPresetFile::set_per_pixel (const QString & code)
{
    m_per_pixel = code;
    m_lines.erase (std::remove_if (m_lines.begin (), m_lines.end (),
        [](const QString & l) {
            return l.trimmed ().contains (QRegularExpression ("^per_pixel_\\d+="));
        }), m_lines.end ());
    QStringList parts = code.split ("\n");
    for (int i = 0; i < parts.size (); i++)
        m_lines.append (QString ("per_pixel_%1=%2").arg (i + 1).arg (parts[i]));
}


/* ─── Preset Browser Widget ────────────────────────────────────────── */

class PresetBrowser : public QMainWindow
{
    Q_OBJECT

public:
    explicit PresetBrowser (QWidget * parent = nullptr);
    ~PresetBrowser ();

    void set_preset_paths (const QStringList & paths);
    void refresh_presets ();
    void load_preset (const QString & path);

signals:
    void preset_selected (const QString & path);

private slots:
    void on_search_changed (const QString & text);
    void on_preset_double_clicked (QListWidgetItem * item);
    void on_featured_clicked ();
    void on_refresh_clicked ();
    void on_load_clicked ();

private:
    void closeEvent (QCloseEvent * event) override;

    QLineEdit * m_search_edit;
    QListWidget * m_preset_list;
    QPushButton * m_featured_btn;
    QPushButton * m_refresh_btn;
    QPushButton * m_load_btn;
    QLabel * m_count_label;

    QStringList m_preset_paths;
    QStringList m_all_presets;  // Full list
    QMap<QString, QString> m_preset_map;  // Display name -> full path

    void rebuild_preset_list ();
    QStringList scan_presets ();
};

PresetBrowser::PresetBrowser (QWidget * parent)
    : QMainWindow (parent)
{
    setWindowTitle (tr ("ProjectM Preset Browser"));
    setWindowIcon (QIcon::fromTheme ("media-optical"));
    resize (400, 500);
    setAttribute (Qt::WA_DeleteOnClose, false);

    auto * central = new QWidget (this);
    auto * layout = new QVBoxLayout (central);
    layout->setContentsMargins (5, 5, 5, 5);

    /* Search box */
    auto * search_label = new QLabel (tr ("Search:"));
    m_search_edit = new QLineEdit ();
    m_search_edit->setPlaceholderText (tr ("Type to filter presets..."));
    auto * search_layout = new QHBoxLayout;
    search_layout->addWidget (search_label);
    search_layout->addWidget (m_search_edit);
    layout->addLayout (search_layout);

    /* Preset list */
    m_preset_list = new QListWidget ();
    m_preset_list->setSelectionMode (QAbstractItemView::SingleSelection);
    layout->addWidget (m_preset_list);

    /* Info bar */
    auto * info_layout = new QHBoxLayout;
    m_count_label = new QLabel ();
    m_count_label->setStyleSheet ("color: #666; font-size: 10pt;");
    info_layout->addWidget (m_count_label);
    info_layout->addStretch ();
    layout->addLayout (info_layout);

    /* Action buttons */
    auto * btn_layout = new QHBoxLayout ();
    m_featured_btn = new QPushButton (tr ("📌 Featured"));
    m_featured_btn->setToolTip (tr ("Load featured presets collection"));
    m_refresh_btn = new QPushButton (tr ("🔄 Refresh"));
    m_refresh_btn->setToolTip (tr ("Rescan preset directories"));
    m_load_btn = new QPushButton (tr ("▶ Load"));
    m_load_btn->setToolTip (tr ("Load selected preset (or double-click)"));
    btn_layout->addWidget (m_featured_btn);
    btn_layout->addWidget (m_refresh_btn);
    btn_layout->addStretch ();
    btn_layout->addWidget (m_load_btn);
    layout->addLayout (btn_layout);

    central->setLayout (layout);
    setCentralWidget (central);

    connect (m_search_edit, &QLineEdit::textChanged, this, &PresetBrowser::on_search_changed);
    connect (m_preset_list, &QListWidget::itemDoubleClicked, this, &PresetBrowser::on_preset_double_clicked);
    connect (m_featured_btn, &QPushButton::clicked, this, &PresetBrowser::on_featured_clicked);
    connect (m_refresh_btn, &QPushButton::clicked, this, &PresetBrowser::on_refresh_clicked);
    connect (m_load_btn, &QPushButton::clicked, this, &PresetBrowser::on_load_clicked);
}

PresetBrowser::~PresetBrowser ()
{
    s_browser = nullptr;
}

void PresetBrowser::closeEvent (QCloseEvent * event)
{
    /* Hide instead of close, so it persists */
    hide ();
    event->ignore ();
}

void PresetBrowser::set_preset_paths (const QStringList & paths)
{
    m_preset_paths = paths;
    refresh_presets ();
}

void PresetBrowser::refresh_presets ()
{
    m_all_presets = scan_presets ();
    rebuild_preset_list ();
}

QStringList PresetBrowser::scan_presets ()
{
    QStringList result;
    m_preset_map.clear ();

    for (const auto & path : m_preset_paths)
    {
        QDir dir (path);
        if (! dir.exists ())
            continue;

        QStringList filters;
        filters << "*.milk" << "*.prjm";
        auto files = dir.entryList (filters, QDir::Files, QDir::Name);

        for (const auto & file : files)
        {
            QString full_path = dir.filePath (file);
            m_preset_map[file] = full_path;
            result.append (file);
        }
    }

    std::sort (result.begin (), result.end ());
    return result;
}

void PresetBrowser::rebuild_preset_list ()
{
    QString query = m_search_edit->text ().toLower ();
    m_preset_list->clear ();

    for (const auto & preset : m_all_presets)
    {
        if (query.isEmpty () || preset.toLower ().contains (query))
            m_preset_list->addItem (preset);
    }

    m_count_label->setText (tr ("Presets: %1 of %2").arg (m_preset_list->count ()).arg (m_all_presets.size ()));
}

void PresetBrowser::on_search_changed (const QString &)
{
    rebuild_preset_list ();
}

void PresetBrowser::on_preset_double_clicked (QListWidgetItem * item)
{
    QString preset_name = item->text ();
    if (m_preset_map.contains (preset_name))
        emit preset_selected (m_preset_map[preset_name]);
}

void PresetBrowser::on_featured_clicked ()
{
    QString featured_dir = QDir::homePath () + "/.local/share/projectM/presets/featured";
    if (QDir (featured_dir).exists ())
    {
        m_search_edit->clear ();
        set_preset_paths ({featured_dir});
    }
}

void PresetBrowser::on_refresh_clicked ()
{
    refresh_presets ();
}

void PresetBrowser::on_load_clicked ()
{
    auto item = m_preset_list->currentItem ();
    if (item)
        on_preset_double_clicked (item);
}

void PresetBrowser::load_preset (const QString & path)
{
    emit preset_selected (path);
}



/* ─── Preset Variable Editor Dialog ────────────────────────────────── */

class PresetVarEditor : public QDialog
{
    Q_OBJECT

public:
    explicit PresetVarEditor (const QString & presetPath, QWidget * parent = nullptr);

signals:
    void preset_modified (const QString & path);

private slots:
    void on_apply ();
    void on_save ();
    void on_save_as ();

private:
    MilkdropPresetFile m_preset;
    QTableWidget * m_var_table;
    QTextEdit * m_pfi_edit;
    QTextEdit * m_pf_edit;
    QTextEdit * m_pp_edit;
    QTextEdit * m_warp_edit;
    QTextEdit * m_comp_edit;
    QLabel * m_status;

    void populate_table ();
    void collect_vars ();

    /* Well-known Milkdrop variables with ranges/descriptions */
    struct VarInfo { QString desc; double min; double max; };
    static const QMap<QString, VarInfo> & known_vars ();
};

const QMap<QString, PresetVarEditor::VarInfo> & PresetVarEditor::known_vars ()
{
    static QMap<QString, VarInfo> m = {
        {"zoom",       {"Zoom amount per frame",              0.5,  2.0}},
        {"rot",        {"Rotation per frame (radians)",      -1.57, 1.57}},
        {"warp",       {"Warp amount",                        0.0,  2.0}},
        {"cx",         {"Center of rotation X",               0.0,  1.0}},
        {"cy",         {"Center of rotation Y",               0.0,  1.0}},
        {"dx",         {"Horizontal motion per frame",       -0.1,  0.1}},
        {"dy",         {"Vertical motion per frame",         -0.1,  0.1}},
        {"sx",         {"Horizontal stretch",                 0.9,  1.1}},
        {"sy",         {"Vertical stretch",                   0.9,  1.1}},
        {"decay",      {"Colour decay (0=none, 1=total)",     0.9,  1.0}},
        {"gamma",      {"Gamma correction",                   1.0,  5.0}},
        {"echo_zoom",  {"Echo effect zoom",                   0.5,  2.0}},
        {"echo_alpha", {"Echo effect alpha",                  0.0,  1.0}},
        {"echo_orient",{"Echo orientation",                   0.0,  3.0}},
        {"wave_mode",  {"Waveform display mode",              0.0,  7.0}},
        {"wave_a",     {"Waveform alpha (opacity)",           0.0,  1.0}},
        {"wave_r",     {"Waveform red",                       0.0,  1.0}},
        {"wave_g",     {"Waveform green",                     0.0,  1.0}},
        {"wave_b",     {"Waveform blue",                      0.0,  1.0}},
        {"wave_x",     {"Waveform X position",                0.0,  1.0}},
        {"wave_y",     {"Waveform Y position",                0.0,  1.0}},
        {"wave_scale", {"Waveform scale",                     0.01, 10.0}},
        {"wave_smoothing", {"Waveform smoothing",             0.0,  1.0}},
        {"ob_size",    {"Outer border size",                  0.0,  0.5}},
        {"ob_r",       {"Outer border red",                   0.0,  1.0}},
        {"ob_g",       {"Outer border green",                 0.0,  1.0}},
        {"ob_b",       {"Outer border blue",                  0.0,  1.0}},
        {"ob_a",       {"Outer border alpha",                 0.0,  1.0}},
        {"ib_size",    {"Inner border size",                  0.0,  0.5}},
        {"ib_r",       {"Inner border red",                   0.0,  1.0}},
        {"ib_g",       {"Inner border green",                 0.0,  1.0}},
        {"ib_b",       {"Inner border blue",                  0.0,  1.0}},
        {"ib_a",       {"Inner border alpha",                 0.0,  1.0}},
        {"mv_x",       {"Motion vector X count",              0.0, 64.0}},
        {"mv_y",       {"Motion vector Y count",              0.0, 48.0}},
        {"mv_dx",      {"Motion vector X offset",            -1.0,  1.0}},
        {"mv_dy",      {"Motion vector Y offset",            -1.0,  1.0}},
        {"mv_l",       {"Motion vector length",               0.0,  5.0}},
        {"mv_r",       {"Motion vector red",                  0.0,  1.0}},
        {"mv_g",       {"Motion vector green",                0.0,  1.0}},
        {"mv_b",       {"Motion vector blue",                 0.0,  1.0}},
        {"mv_a",       {"Motion vector alpha",                0.0,  1.0}},
        {"fShader",    {"Shader version (float)",             0.0,  3.0}},
        {"nVideoEchoOrientation",  {"Video echo orientation", 0.0,  3.0}},
        {"fVideoEchoZoom",         {"Video echo zoom",        0.5,  2.0}},
        {"fVideoEchoAlpha",        {"Video echo alpha",       0.0,  1.0}},
        {"nWaveMode",              {"Wave display mode",      0.0,  7.0}},
        {"bAdditiveWaves",         {"Additive waveforms",     0.0,  1.0}},
        {"bWaveDots",              {"Draw wave as dots",      0.0,  1.0}},
        {"bWaveThick",             {"Thick waveform",         0.0,  1.0}},
        {"bModWaveAlphaByVolume",  {"Mod wave alpha by vol",  0.0,  1.0}},
        {"bMaximizeWaveColor",     {"Maximize wave color",    0.0,  1.0}},
        {"bTexWrap",               {"Texture wrapping",       0.0,  1.0}},
        {"bDarkenCenter",          {"Darken center",          0.0,  1.0}},
        {"bRedBlueStereo",         {"Red/blue stereo",        0.0,  1.0}},
        {"bBrighten",              {"Brighten effect",        0.0,  1.0}},
        {"bDarken",                {"Darken effect",          0.0,  1.0}},
        {"bSolarize",              {"Solarize effect",        0.0,  1.0}},
        {"bInvert",                {"Invert colors",          0.0,  1.0}},
        {"fWarpAnimSpeed",         {"Warp animation speed",   0.0, 10.0}},
        {"fWarpScale",             {"Warp scale",             0.0, 10.0}},
        {"fDecay",                 {"Frame decay",            0.9,  1.0}},
        {"fGammaAdj",              {"Gamma adjustment",       0.5,  5.0}},
        {"fRating",                {"Preset rating",          0.0,  5.0}},
    };
    return m;
}

PresetVarEditor::PresetVarEditor (const QString & presetPath, QWidget * parent)
    : QDialog (parent)
{
    setWindowTitle (tr ("Preset Variable Editor – %1").arg (QFileInfo (presetPath).baseName ()));
    resize (800, 600);

    if (! m_preset.load (presetPath))
    {
        QMessageBox::warning (this, tr ("Error"), tr ("Could not open preset file:\n%1").arg (presetPath));
        return;
    }

    auto * layout = new QVBoxLayout (this);

    auto * tabs = new QTabWidget ();
    layout->addWidget (tabs);

    /* ── Tab 1: General Variables ── */
    auto * var_tab = new QWidget ();
    auto * var_layout = new QVBoxLayout (var_tab);

    m_var_table = new QTableWidget ();
    m_var_table->setColumnCount (4);
    m_var_table->setHorizontalHeaderLabels ({tr ("Variable"), tr ("Value"), tr ("Range"), tr ("Description")});
    m_var_table->horizontalHeader ()->setStretchLastSection (true);
    m_var_table->horizontalHeader ()->setSectionResizeMode (0, QHeaderView::ResizeToContents);
    m_var_table->horizontalHeader ()->setSectionResizeMode (1, QHeaderView::Interactive);
    m_var_table->setSelectionBehavior (QAbstractItemView::SelectRows);
    m_var_table->setSortingEnabled (true);

    populate_table ();
    var_layout->addWidget (m_var_table);
    tabs->addTab (var_tab, tr ("Variables"));

    /* ── Tab 2: Per-Frame Init ── */
    m_pfi_edit = new QTextEdit ();
    m_pfi_edit->setFontFamily ("monospace");
    m_pfi_edit->setPlainText (m_preset.per_frame_init ());
    tabs->addTab (m_pfi_edit, tr ("Per-Frame Init"));

    /* ── Tab 3: Per-Frame ── */
    m_pf_edit = new QTextEdit ();
    m_pf_edit->setFontFamily ("monospace");
    m_pf_edit->setPlainText (m_preset.per_frame ());
    tabs->addTab (m_pf_edit, tr ("Per-Frame"));

    /* ── Tab 4: Per-Pixel ── */
    m_pp_edit = new QTextEdit ();
    m_pp_edit->setFontFamily ("monospace");
    m_pp_edit->setPlainText (m_preset.per_pixel ());
    tabs->addTab (m_pp_edit, tr ("Per-Pixel"));

    /* ── Tab 5: Warp Shader ── */
    m_warp_edit = new QTextEdit ();
    m_warp_edit->setFontFamily ("monospace");
    m_warp_edit->setPlainText (m_preset.warp_shader ());
    m_warp_edit->setReadOnly (true); /* shaders are complex, read-only for now */
    tabs->addTab (m_warp_edit, tr ("Warp Shader (readonly)"));

    /* ── Tab 6: Composite Shader ── */
    m_comp_edit = new QTextEdit ();
    m_comp_edit->setFontFamily ("monospace");
    m_comp_edit->setPlainText (m_preset.comp_shader ());
    m_comp_edit->setReadOnly (true);
    tabs->addTab (m_comp_edit, tr ("Comp Shader (readonly)"));

    /* ── Buttons ── */
    m_status = new QLabel ();
    layout->addWidget (m_status);

    auto * btn_layout = new QHBoxLayout ();
    auto * apply_btn = new QPushButton (tr ("Apply && Reload"));
    auto * save_btn  = new QPushButton (tr ("Save"));
    auto * saveas_btn = new QPushButton (tr ("Save As..."));
    auto * close_btn = new QPushButton (tr ("Close"));
    btn_layout->addStretch ();
    btn_layout->addWidget (apply_btn);
    btn_layout->addWidget (save_btn);
    btn_layout->addWidget (saveas_btn);
    btn_layout->addWidget (close_btn);
    layout->addLayout (btn_layout);

    connect (apply_btn, &QPushButton::clicked, this, &PresetVarEditor::on_apply);
    connect (save_btn,  &QPushButton::clicked, this, &PresetVarEditor::on_save);
    connect (saveas_btn, &QPushButton::clicked, this, &PresetVarEditor::on_save_as);
    connect (close_btn, &QPushButton::clicked, this, &QDialog::close);
}

void PresetVarEditor::populate_table ()
{
    auto vars = m_preset.general_vars ();
    const auto & kv = known_vars ();

    m_var_table->setRowCount (vars.size ());

    int row = 0;
    for (auto it = vars.constBegin (); it != vars.constEnd (); ++it, ++row)
    {
        auto * key_item = new QTableWidgetItem (it.key ());
        key_item->setFlags (key_item->flags () & ~Qt::ItemIsEditable);
        m_var_table->setItem (row, 0, key_item);

        m_var_table->setItem (row, 1, new QTableWidgetItem (it.value ()));

        QString range, desc;
        if (kv.contains (it.key ()))
        {
            const auto & info = kv[it.key ()];
            range = QString ("[%1 .. %2]").arg (info.min).arg (info.max);
            desc = info.desc;
        }

        auto * range_item = new QTableWidgetItem (range);
        range_item->setFlags (range_item->flags () & ~Qt::ItemIsEditable);
        m_var_table->setItem (row, 2, range_item);

        auto * desc_item = new QTableWidgetItem (desc);
        desc_item->setFlags (desc_item->flags () & ~Qt::ItemIsEditable);
        m_var_table->setItem (row, 3, desc_item);
    }
}

void PresetVarEditor::collect_vars ()
{
    /* Collect general variable edits from the table */
    for (int row = 0; row < m_var_table->rowCount (); row++)
    {
        QString key = m_var_table->item (row, 0)->text ();
        QString val = m_var_table->item (row, 1)->text ();
        m_preset.set_general_var (key, val);
    }

    /* Collect equation edits */
    m_preset.set_per_frame_init (m_pfi_edit->toPlainText ());
    m_preset.set_per_frame (m_pf_edit->toPlainText ());
    m_preset.set_per_pixel (m_pp_edit->toPlainText ());
}

void PresetVarEditor::on_apply ()
{
    collect_vars ();
    /* Save to temp, reload in projectM */
    QString tmp = m_preset.path () + ".tmp";
    if (m_preset.save (tmp))
    {
        emit preset_modified (tmp);
        m_status->setText (tr ("Applied. Preset reloaded with changes."));
    }
    else
        m_status->setText (tr ("Error writing temporary preset file."));
}

void PresetVarEditor::on_save ()
{
    collect_vars ();
    if (m_preset.save ())
        m_status->setText (tr ("Saved to %1").arg (m_preset.path ()));
    else
        m_status->setText (tr ("Error saving preset file."));
}

void PresetVarEditor::on_save_as ()
{
    collect_vars ();
    QString path = QFileDialog::getSaveFileName (this,
        tr ("Save Preset As"), m_preset.path (),
        tr ("Milkdrop Presets (*.milk *.prjm);;All Files (*)"));
    if (! path.isEmpty ())
    {
        if (m_preset.save (path))
            m_status->setText (tr ("Saved to %1").arg (path));
        else
            m_status->setText (tr ("Error saving preset file."));
    }
}


/* ─── OpenGL rendering widget ──────────────────────────────────────── */

class ProjectMWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit ProjectMWidget (QWidget * parent = nullptr);
    ~ProjectMWidget ();

    void feed_pcm (const float * pcm, int samples);
    void clear_vis ();

    void set_preset_path (const QString & path);
    void load_preset_file (const QString & file);
    void next_preset ();
    void prev_preset ();
    void random_preset ();
    void lock_preset (bool lock);
    bool is_locked () const { return m_locked; }

    QString current_preset_name () const;
    QString current_preset_path () const;
    QStringList list_presets () const;
    QStringList list_preset_paths () const;
    int current_preset_index () const;
    void select_preset (int index);

    void set_fps (int fps);
    void set_beat_sensitivity (float s);
    void set_hard_cut_enabled (bool e);
    void set_hard_cut_duration (float d);
    void set_soft_cut_duration (float d);

signals:
    void preset_changed (const QString & name);

protected:
    void initializeGL () override;
    void resizeGL (int w, int h) override;
    void paintGL () override;
    void keyPressEvent (QKeyEvent * e) override;

private:
    projectm_handle m_pm = nullptr;
    projectm_playlist_handle m_playlist = nullptr;
    QTimer * m_timer = nullptr;
    bool m_locked = false;
    bool m_initialized = false;
    QString m_preset_path;
    int m_target_fps = 60;

    void setup_projectm ();
    void cleanup_projectm ();
};


/* ─── Container widget with controls ───────────────────────────────── */

class ProjectMContainer : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectMContainer (QWidget * parent = nullptr);
    ~ProjectMContainer ();

    ProjectMWidget * vis_widget () const { return m_vis; }
    void apply_runtime_settings ();

    /* Audacious menu actions */
    void action_next ()           { m_vis->next_preset (); }
    void action_prev ()           { m_vis->prev_preset (); }
    void action_random ()         { m_vis->random_preset (); }
    void action_lock ()           { m_vis->lock_preset (! m_vis->is_locked ()); update_lock_ui (); }
    void action_edit_preset ();
    void action_browse_presets ();
    void action_fullscreen ();

private slots:
    void on_preset_selected (int row);
    void on_preset_changed (const QString & name);
    void on_search_changed (const QString & text);
    void on_preset_modified (const QString & path);

private:
    ProjectMWidget * m_vis;
    QListWidget * m_preset_list;
    QLabel * m_current_label;
    QPushButton * m_lock_btn;
    QLineEdit * m_path_edit;
    QLineEdit * m_search_edit;

    QStringList m_all_preset_names;
    QStringList m_all_preset_paths;

    void update_lock_ui ();
    void refresh_preset_list ();
};


/* ─── Plugin declaration ───────────────────────────────────────────── */

static const char pm_about[] =
    N_("ProjectM / Milkdrop Visualizer for Audacious\n"
       "Copyright 2026\n\n"
       "Uses libprojectM 4.x\n"
       "https://github.com/projectM-visualizer/projectm\n\n"
       "License: GPLv2+");

static const PreferencesWidget pm_prefs_widgets[] = {
    WidgetFileEntry (N_("Preset directory"),
        WidgetString (PM_CFG_SECTION, "preset_path", pm_settings_changed),
        {FileSelectMode::Folder}),
    WidgetSpin (N_("Target FPS"),
        WidgetInt (PM_CFG_SECTION, "fps", pm_settings_changed),
        {15, 120, 1, nullptr}),
    WidgetSpin (N_("Beat sensitivity (%)"),
        WidgetInt (PM_CFG_SECTION, "beat_sensitivity", pm_settings_changed),
        {0, 500, 1, "%"}),
    WidgetCheck (N_("Enable hard cuts"),
        WidgetBool (PM_CFG_SECTION, "hard_cut", pm_settings_changed)),
    WidgetSpin (N_("Hard cut duration"),
        WidgetFloat (PM_CFG_SECTION, "hard_cut_duration", pm_settings_changed),
        {1, 120, 0.5, "s"}),
    WidgetSpin (N_("Soft cut duration"),
        WidgetFloat (PM_CFG_SECTION, "soft_cut_duration", pm_settings_changed),
        {1, 120, 0.5, "s"})
};

static const PluginPreferences pm_prefs = {
    pm_prefs_widgets,
    nullptr,
    pm_settings_changed,
    nullptr
};

static void pm_settings_changed ()
{
    if (s_container)
        s_container->apply_runtime_settings ();
}

/* Menu actions – registered in Audacious' Visualization menu */
static void menu_next ()           { if (s_container) s_container->action_next (); }
static void menu_prev ()           { if (s_container) s_container->action_prev (); }
static void menu_random ()         { if (s_container) s_container->action_random (); }
static void menu_lock ()           { if (s_container) s_container->action_lock (); }
static void menu_edit_preset ()    { if (s_container) s_container->action_edit_preset (); }
static void menu_browse ()         { if (s_container) s_container->action_browse_presets (); }
static void menu_browser ()        {
    if (! s_browser)
    {
        s_browser = new PresetBrowser ();
        
        /* Wire up preset loading to the visualization container */
        if (s_container)
            QObject::connect (s_browser, QOverload<const QString &>::of (&PresetBrowser::preset_selected),
                             s_container->vis_widget (), &ProjectMWidget::load_preset_file);
    }
    
    /* Initialize browser with local preset paths */
    QStringList paths;
    paths << (QDir::homePath () + "/.local/share/projectM/presets");
    paths << "/usr/share/projectM/presets";
    paths << "/usr/local/share/projectM/presets";
    s_browser->set_preset_paths (paths);
    
    s_browser->show ();
    s_browser->raise ();
    s_browser->activateWindow ();
}
static void menu_fullscreen ()     { if (s_container) s_container->action_fullscreen (); }

static const AudMenuID pm_menu = AudMenuID::Main;

class ProjectMVis : public VisPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("ProjectM (Milkdrop)"),
        PACKAGE,
        pm_about,
        & pm_prefs,
        PluginQtOnly
    };

    constexpr ProjectMVis () : VisPlugin (info, Visualizer::MonoPCM) {}

    bool init ();
    void cleanup ();
    void * get_qt_widget ();
    void clear ();
    void render_mono_pcm (const float * pcm);
};

extern "C" ProjectMVis aud_plugin_instance;
ProjectMVis aud_plugin_instance;


/* ================================================================== */
/*  ProjectMWidget implementation                                      */
/* ================================================================== */

ProjectMWidget::ProjectMWidget (QWidget * parent)
    : QOpenGLWidget (parent)
{
    setMinimumSize (320, 240);
    setFocusPolicy (Qt::StrongFocus);

    m_timer = new QTimer (this);
    connect (m_timer, &QTimer::timeout, this, QOverload<>::of(&QOpenGLWidget::update));
}

ProjectMWidget::~ProjectMWidget ()
{
    m_timer->stop ();
    makeCurrent ();
    cleanup_projectm ();
    doneCurrent ();
}

void ProjectMWidget::setup_projectm ()
{
    if (m_pm) cleanup_projectm ();

    m_pm = projectm_create ();
    if (! m_pm) { qWarning ("ProjectM: failed to create instance"); return; }

    projectm_set_window_size (m_pm, width (), height ());
    projectm_set_fps (m_pm, m_target_fps);
    projectm_set_mesh_size (m_pm, 48, 32);
    projectm_set_beat_sensitivity (m_pm, 1.0f);
    projectm_set_hard_cut_enabled (m_pm, true);
    projectm_set_hard_cut_duration (m_pm, 20.0f);
    projectm_set_soft_cut_duration (m_pm, 10.0f);
    projectm_set_aspect_correction (m_pm, true);

    std::vector<QByteArray> texture_bytes;
    std::vector<const char *> texture_paths;
    for (const auto & path : pm_texture_search_paths ())
    {
        if (! QDir (path).exists ())
            continue;

        texture_bytes.push_back (path.toUtf8 ());
        texture_paths.push_back (texture_bytes.back ().constData ());
    }

    if (! texture_paths.empty ())
        projectm_set_texture_search_paths (m_pm, texture_paths.data (), texture_paths.size ());

    m_playlist = projectm_playlist_create (m_pm);
    m_initialized = true;
}

void ProjectMWidget::cleanup_projectm ()
{
    if (m_playlist) { projectm_playlist_destroy (m_playlist); m_playlist = nullptr; }
    if (m_pm)       { projectm_destroy (m_pm); m_pm = nullptr; }
    m_initialized = false;
}

void ProjectMWidget::initializeGL ()
{
    initializeOpenGLFunctions ();
    glClearColor (0, 0, 0, 1);
    setup_projectm ();
    m_timer->start (1000 / m_target_fps);

    /* Auto-detect preset directories */
    QStringList search = {
        QDir::homePath () + "/.local/share/projectM/presets/featured",
        "/usr/share/projectM/presets/featured",
        "/usr/share/projectM/presets",
        "/usr/local/share/projectM/presets",
        "/usr/share/projectm-presets",
        QDir::homePath () + "/.projectM/presets",
        QDir::homePath () + "/.local/share/projectM/presets"
    };
    for (const auto & p : search)
        if (QDir (p).exists ()) { set_preset_path (p); break; }
}

void ProjectMWidget::resizeGL (int w, int h)
{
    if (m_pm) projectm_set_window_size (m_pm, w, h);
}

void ProjectMWidget::paintGL ()
{
    if (! m_pm || ! m_initialized) { glClear (GL_COLOR_BUFFER_BIT); return; }
    projectm_opengl_render_frame (m_pm);
}

void ProjectMWidget::keyPressEvent (QKeyEvent * e)
{
    switch (e->key ())
    {
    case Qt::Key_N: case Qt::Key_Right: next_preset (); break;
    case Qt::Key_P: case Qt::Key_Left:  prev_preset (); break;
    case Qt::Key_R: random_preset (); break;
    case Qt::Key_L: lock_preset (! m_locked); break;
    case Qt::Key_E:
        if (s_container) s_container->action_edit_preset ();
        break;
    case Qt::Key_F:
        if (auto * w = window ())
            w->isFullScreen () ? w->showNormal () : w->showFullScreen ();
        break;
    default: QOpenGLWidget::keyPressEvent (e); break;
    }
}

void ProjectMWidget::feed_pcm (const float * pcm, int samples)
{
    if (m_pm && m_initialized)
        projectm_pcm_add_float (m_pm, pcm, samples, PROJECTM_MONO);
}

void ProjectMWidget::clear_vis () { /* projectM handles silence fine */ }

void ProjectMWidget::set_preset_path (const QString & path)
{
    if (! m_playlist || ! m_pm) return;
    m_preset_path = path;
    projectm_playlist_clear (m_playlist);
    QByteArray pb = path.toUtf8 ();
    projectm_playlist_add_path (m_playlist, pb.constData (), true, false);
    projectm_playlist_sort (m_playlist, 0, projectm_playlist_size (m_playlist),
        SORT_PREDICATE_FILENAME_ONLY,
        SORT_ORDER_ASCENDING);
    projectm_playlist_set_shuffle (m_playlist, true);
    if (projectm_playlist_size (m_playlist) > 0)
        projectm_playlist_play_next (m_playlist, true);
}

void ProjectMWidget::load_preset_file (const QString & file)
{
    if (! m_pm) return;
    QByteArray fb = file.toUtf8 ();
    projectm_load_preset_file (m_pm, fb.constData (), false);
    emit preset_changed (QFileInfo (file).baseName ());
}

void ProjectMWidget::next_preset ()
{
    if (m_playlist && projectm_playlist_size (m_playlist) > 0) {
        projectm_playlist_play_next (m_playlist, true);
        emit preset_changed (current_preset_name ());
    }
}

void ProjectMWidget::prev_preset ()
{
    if (m_playlist && projectm_playlist_size (m_playlist) > 0) {
        projectm_playlist_play_previous (m_playlist, true);
        emit preset_changed (current_preset_name ());
    }
}

void ProjectMWidget::random_preset ()
{
    if (m_playlist && projectm_playlist_size (m_playlist) > 0) {
        unsigned int n = projectm_playlist_size (m_playlist);
        projectm_playlist_set_position (m_playlist, rand () % n, true);
        emit preset_changed (current_preset_name ());
    }
}

void ProjectMWidget::lock_preset (bool lock)
{
    m_locked = lock;
    if (m_playlist) projectm_playlist_set_shuffle (m_playlist, ! lock);
}

QString ProjectMWidget::current_preset_name () const
{
    if (! m_playlist) return {};
    unsigned int idx = projectm_playlist_get_position (m_playlist);
    char * fn = projectm_playlist_item (m_playlist, idx);
    if (! fn) return {};
    QString name = QFileInfo (QString::fromUtf8 (fn)).baseName ();
    projectm_playlist_free_string (fn);
    return name;
}

QString ProjectMWidget::current_preset_path () const
{
    if (! m_playlist) return {};
    unsigned int idx = projectm_playlist_get_position (m_playlist);
    char * fn = projectm_playlist_item (m_playlist, idx);
    if (! fn) return {};
    QString path = QString::fromUtf8 (fn);
    projectm_playlist_free_string (fn);
    return path;
}

QStringList ProjectMWidget::list_presets () const
{
    QStringList r;
    if (! m_playlist) return r;
    unsigned int n = projectm_playlist_size (m_playlist);
    for (unsigned int i = 0; i < n; i++) {
        char * fn = projectm_playlist_item (m_playlist, i);
        if (fn) { r.append (QFileInfo (QString::fromUtf8 (fn)).baseName ()); projectm_playlist_free_string (fn); }
    }
    return r;
}

QStringList ProjectMWidget::list_preset_paths () const
{
    QStringList r;
    if (! m_playlist) return r;
    unsigned int n = projectm_playlist_size (m_playlist);
    for (unsigned int i = 0; i < n; i++) {
        char * fn = projectm_playlist_item (m_playlist, i);
        if (fn) { r.append (QString::fromUtf8 (fn)); projectm_playlist_free_string (fn); }
    }
    return r;
}

int ProjectMWidget::current_preset_index () const
{
    return m_playlist ? (int) projectm_playlist_get_position (m_playlist) : -1;
}

void ProjectMWidget::select_preset (int index)
{
    if (! m_playlist || index < 0) return;
    if ((unsigned) index >= projectm_playlist_size (m_playlist)) return;
    projectm_playlist_set_position (m_playlist, (unsigned) index, true);
    emit preset_changed (current_preset_name ());
}

void ProjectMWidget::set_fps (int fps)
{
    m_target_fps = fps;
    if (m_pm) projectm_set_fps (m_pm, fps);
    if (m_timer->isActive ()) m_timer->setInterval (1000 / fps);
}

void ProjectMWidget::set_beat_sensitivity (float s)
{   if (m_pm) projectm_set_beat_sensitivity (m_pm, s); }
void ProjectMWidget::set_hard_cut_enabled (bool e)
{   if (m_pm) projectm_set_hard_cut_enabled (m_pm, e); }
void ProjectMWidget::set_hard_cut_duration (float d)
{   if (m_pm) projectm_set_hard_cut_duration (m_pm, d); }
void ProjectMWidget::set_soft_cut_duration (float d)
{   if (m_pm) projectm_set_soft_cut_duration (m_pm, d); }


/* ================================================================== */
/*  ProjectMContainer implementation                                   */
/* ================================================================== */

ProjectMContainer::ProjectMContainer (QWidget * parent)
    : QWidget (parent)
{
    auto * main_layout = new QHBoxLayout (this);

    m_vis = new ProjectMWidget (this);

    /* Right panel */
    auto * panel = new QWidget;
    panel->setFixedWidth (300);
    auto * pl = new QVBoxLayout (panel);

    /* Current preset */
    m_current_label = new QLabel (tr ("No preset loaded"));
    m_current_label->setWordWrap (true);
    m_current_label->setStyleSheet ("font-weight:bold; padding:4px;");
    pl->addWidget (m_current_label);

    /* Preset path */
    auto * path_row = new QHBoxLayout;
    m_path_edit = new QLineEdit;
    m_path_edit->setPlaceholderText (tr ("Preset directory..."));
    m_path_edit->setReadOnly (true);
    auto * browse_btn = new QPushButton (tr ("Browse..."));
    path_row->addWidget (m_path_edit);
    path_row->addWidget (browse_btn);
    pl->addLayout (path_row);

    /* Navigation */
    auto * nav = new QHBoxLayout;
    auto * prev_btn  = new QPushButton (tr ("◀"));
    auto * next_btn  = new QPushButton (tr ("▶"));
    auto * rand_btn  = new QPushButton (tr ("⟳"));
    m_lock_btn       = new QPushButton (tr ("🔓"));
    m_lock_btn->setCheckable (true);
    auto * edit_btn  = new QPushButton (tr ("Edit"));
    prev_btn->setToolTip (tr ("Previous preset (P/←)"));
    next_btn->setToolTip (tr ("Next preset (N/→)"));
    rand_btn->setToolTip (tr ("Random preset (R)"));
    m_lock_btn->setToolTip (tr ("Lock/unlock preset (L)"));
    edit_btn->setToolTip (tr ("Edit preset variables (E)"));
    nav->addWidget (prev_btn);
    nav->addWidget (next_btn);
    nav->addWidget (rand_btn);
    nav->addWidget (m_lock_btn);
    nav->addWidget (edit_btn);
    pl->addLayout (nav);

    /* Search */
    m_search_edit = new QLineEdit;
    m_search_edit->setPlaceholderText (tr ("Search presets..."));
    m_search_edit->setClearButtonEnabled (true);
    pl->addWidget (m_search_edit);

    /* Preset list */
    m_preset_list = new QListWidget;
    pl->addWidget (m_preset_list, 1);

    /* Shortcuts info */
    auto * info = new QLabel (tr (
        "<small><b>Keys:</b> N/→ Next · P/← Prev · R Random<br>"
        "L Lock · E Edit · F Fullscreen</small>"));
    info->setWordWrap (true);
    pl->addWidget (info);

    /* Splitter */
    auto * splitter = new QSplitter (Qt::Horizontal);
    splitter->addWidget (m_vis);
    splitter->addWidget (panel);
    splitter->setStretchFactor (0, 1);
    splitter->setStretchFactor (1, 0);
    main_layout->addWidget (splitter);

    /* Connections */
    connect (browse_btn, &QPushButton::clicked, this, &ProjectMContainer::action_browse_presets);
    connect (prev_btn,  &QPushButton::clicked, [this]{ action_prev (); });
    connect (next_btn,  &QPushButton::clicked, [this]{ action_next (); });
    connect (rand_btn,  &QPushButton::clicked, [this]{ action_random (); });
    connect (m_lock_btn, &QPushButton::toggled, [this](bool c){ m_vis->lock_preset (c); update_lock_ui (); });
    connect (edit_btn,  &QPushButton::clicked, this, &ProjectMContainer::action_edit_preset);
    connect (m_search_edit, &QLineEdit::textChanged, this, &ProjectMContainer::on_search_changed);
    connect (m_preset_list, &QListWidget::currentRowChanged, this, &ProjectMContainer::on_preset_selected);
    connect (m_vis, &ProjectMWidget::preset_changed, this, &ProjectMContainer::on_preset_changed);

    apply_runtime_settings ();
}

ProjectMContainer::~ProjectMContainer ()
{
    s_container = nullptr;
}

void ProjectMContainer::action_browse_presets ()
{
    QString dir = QFileDialog::getExistingDirectory (this, tr ("Select Preset Directory"),
        m_path_edit->text ().isEmpty () ? QDir::homePath () : m_path_edit->text ());
    if (! dir.isEmpty ()) {
        m_path_edit->setText (dir);
        aud_set_str (PM_CFG_SECTION, "preset_path", dir.toUtf8 ().constData ());
        m_vis->set_preset_path (dir);
        refresh_preset_list ();
    }
}

void ProjectMContainer::action_edit_preset ()
{
    QString path = m_vis->current_preset_path ();
    if (path.isEmpty ()) { QMessageBox::information (this, tr ("ProjectM"), tr ("No preset loaded.")); return; }

    auto * editor = new PresetVarEditor (path, this);
    connect (editor, &PresetVarEditor::preset_modified, this, &ProjectMContainer::on_preset_modified);
    editor->show ();
}

void ProjectMContainer::action_fullscreen ()
{
    if (auto * w = window ())
        w->isFullScreen () ? w->showNormal () : w->showFullScreen ();
}

void ProjectMContainer::on_preset_modified (const QString & path)
{
    m_vis->load_preset_file (path);
}

void ProjectMContainer::on_preset_selected (int row)
{
    if (row < 0) return;
    /* Map filtered row back to real index */
    QString name = m_preset_list->item (row)->text ();
    int idx = m_all_preset_names.indexOf (name);
    if (idx >= 0) m_vis->select_preset (idx);
}

void ProjectMContainer::on_preset_changed (const QString & name)
{
    m_current_label->setText (name.isEmpty () ? tr ("No preset loaded") : name);
    /* Highlight without triggering */
    int idx = m_all_preset_names.indexOf (name);
    for (int i = 0; i < m_preset_list->count (); i++) {
        if (m_preset_list->item (i)->text () == name) {
            m_preset_list->blockSignals (true);
            m_preset_list->setCurrentRow (i);
            m_preset_list->blockSignals (false);
            break;
        }
    }
}

void ProjectMContainer::on_search_changed (const QString & text)
{
    m_preset_list->clear ();
    for (const auto & name : m_all_preset_names)
        if (text.isEmpty () || name.contains (text, Qt::CaseInsensitive))
            m_preset_list->addItem (name);
}

void ProjectMContainer::update_lock_ui ()
{
    m_lock_btn->setText (m_vis->is_locked () ? tr ("🔒") : tr ("🔓"));
}

void ProjectMContainer::refresh_preset_list ()
{
    m_all_preset_names = m_vis->list_presets ();
    m_all_preset_paths = m_vis->list_preset_paths ();
    m_preset_list->clear ();
    m_preset_list->addItems (m_all_preset_names);
}

void ProjectMContainer::apply_runtime_settings ()
{
    int fps = aud_get_int (PM_CFG_SECTION, "fps");
    int beat = aud_get_int (PM_CFG_SECTION, "beat_sensitivity");
    bool hard_cut = aud_get_bool (PM_CFG_SECTION, "hard_cut");
    double hard_cut_dur = aud_get_double (PM_CFG_SECTION, "hard_cut_duration");
    double soft_cut_dur = aud_get_double (PM_CFG_SECTION, "soft_cut_duration");

    if (fps < 15 || fps > 120)
        fps = 60;
    if (beat < 0 || beat > 500)
        beat = 100;
    if (hard_cut_dur < 1.0 || hard_cut_dur > 120.0)
        hard_cut_dur = 20.0;
    if (soft_cut_dur < 1.0 || soft_cut_dur > 120.0)
        soft_cut_dur = 10.0;

    m_vis->set_fps (fps);
    m_vis->set_beat_sensitivity ((float) beat / 100.0f);
    m_vis->set_hard_cut_enabled (hard_cut);
    m_vis->set_hard_cut_duration ((float) hard_cut_dur);
    m_vis->set_soft_cut_duration ((float) soft_cut_dur);

    String preset_path = aud_get_str (PM_CFG_SECTION, "preset_path");
    QString qpath = preset_path ? QString::fromUtf8 ((const char *) preset_path) : QString ();
    if (! qpath.isEmpty () && QDir (qpath).exists ()) {
        m_path_edit->setText (qpath);
        QTimer::singleShot (200, this, [this, qpath]{
            m_vis->set_preset_path (qpath);
            refresh_preset_list ();
        });
    }
    else {
        m_path_edit->clear ();
        m_preset_list->clear ();
    }
}


/* ================================================================== */
/*  Plugin interface + Audacious menu integration                      */
/* ================================================================== */

bool ProjectMVis::init ()
{
    aud_config_set_defaults (PM_CFG_SECTION, pm_defaults);

    /* Register menu items under Visualization */
    aud_plugin_menu_add (pm_menu, menu_next, N_("ProjectM: Next Preset"), "go-next");
    aud_plugin_menu_add (pm_menu, menu_prev, N_("ProjectM: Previous Preset"), "go-previous");
    aud_plugin_menu_add (pm_menu, menu_random, N_("ProjectM: Random Preset"), "media-playlist-shuffle");
    aud_plugin_menu_add (pm_menu, menu_lock, N_("ProjectM: Lock/Unlock Preset"), "system-lock-screen");
    aud_plugin_menu_add (pm_menu, menu_edit_preset, N_("ProjectM: Edit Preset Variables..."), "document-edit");
    aud_plugin_menu_add (pm_menu, menu_browse, N_("ProjectM: Browse Preset Directory..."), "folder-open");
    aud_plugin_menu_add (pm_menu, menu_browser, N_("ProjectM: Preset Browser"), "media-optical");
    aud_plugin_menu_add (pm_menu, menu_fullscreen, N_("ProjectM: Toggle Fullscreen"), "view-fullscreen");

    return true;
}

void ProjectMVis::cleanup ()
{
    aud_plugin_menu_remove (pm_menu, menu_next);
    aud_plugin_menu_remove (pm_menu, menu_prev);
    aud_plugin_menu_remove (pm_menu, menu_random);
    aud_plugin_menu_remove (pm_menu, menu_lock);
    aud_plugin_menu_remove (pm_menu, menu_edit_preset);
    aud_plugin_menu_remove (pm_menu, menu_browse);
    aud_plugin_menu_remove (pm_menu, menu_browser);
    aud_plugin_menu_remove (pm_menu, menu_fullscreen);
}

void * ProjectMVis::get_qt_widget ()
{
    s_container = new ProjectMContainer;
    return s_container;
}

void ProjectMVis::clear ()
{
    if (s_container && s_container->vis_widget ())
        s_container->vis_widget ()->clear_vis ();
}

void ProjectMVis::render_mono_pcm (const float * pcm)
{
    if (s_container && s_container->vis_widget ())
        s_container->vis_widget ()->feed_pcm (pcm, 512);
}

#include "projectm-vis.moc"
