/*
 #
 #  File        : gmic_gimp.cpp
 #                ( C++ source file )
 #
 #  Description : G'MIC for GIMP - A plug-in to allow the use
 #                of G'MIC commands in GIMP.
 #
 #  Copyright   : David Tschumperle
 #                ( http://tschumperle.users.greyc.fr/ )
 #
 #  License     : CeCILL v2.0
 #                ( http://www.cecill.info/licences/Licence_CeCILL_V2-en.html )
 #
 #  This software is governed by the CeCILL  license under French law and
 #  abiding by the rules of distribution of free software.  You can  use,
 #  modify and/ or redistribute the software under the terms of the CeCILL
 #  license as circulated by CEA, CNRS and INRIA at the following URL
 #  "http://www.cecill.info".
 #
 #  As a counterpart to the access to the source code and  rights to copy,
 #  modify and redistribute granted by the license, users are provided only
 #  with a limited warranty  and the software's author,  the holder of the
 #  economic rights,  and the successive licensors  have only  limited
 #  liability.
 #
 #  In this respect, the user's attention is drawn to the risks associated
 #  with loading,  using,  modifying and/or developing or reproducing the
 #  software by the user in light of its specific status of free software,
 #  that may mean  that it is complicated to manipulate,  and  that  also
 #  therefore means  that it is reserved for developers  and  experienced
 #  professionals having in-depth computer knowledge. Users are therefore
 #  encouraged to load and test the software's suitability as regards their
 #  requirements in conditions enabling the security of their systems and/or
 #  data to be ensured and, more generally, to use and operate it in the
 #  same conditions as regards security.
 #
 #  The fact that you are presently reading this means that you have had
 #  knowledge of the CeCILL license and that you accept its terms.
 #
*/

// Include necessary header files.
//--------------------------------
#define cimg_display_type 0
#include "gmic.h"
#include "gmic_stdlib.h"
#include <pthread.h>
#include <locale>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#undef min
#undef max

// Define default pixel type.
#ifndef gmic_pixel_type
#define gmic_pixel_type float
#endif
#define __s_gmic_pixel_type(s) #s
#define _s_gmic_pixel_type(s) __s_gmic_pixel_type(s)
#define s_gmic_pixel_type _s_gmic_pixel_type(gmic_pixel_type)

// Manage different versions of the GIMP API.
#define _gimp_item_is_valid gimp_item_is_valid
#define _gimp_image_get_item_position gimp_image_get_item_position
#if GIMP_MINOR_VERSION<=8
#define _gimp_item_get_visible gimp_drawable_get_visible
#else
#define _gimp_item_get_visible gimp_item_get_visible
#endif

using namespace cimg_library;

// Define plug-in global variables.
//---------------------------------
CImgList<char> gmic_entries;                   // The list of recognized G'MIC menu entries.
CImgList<char> gmic_1stlevel_entries;          // The treepath positions of 1st-level G'MIC menu entries.
CImgList<char> gmic_commands;                  // The list of corresponding G'MIC commands to process the image.
CImgList<char> gmic_preview_commands;          // The list of corresponding G'MIC commands to preview the image.
CImgList<char> gmic_arguments;                 // The list of corresponding needed filter arguments.
CImgList<char> gmic_faves;                     // The list of favorites filters and their default parameters.
CImgList<double> gmic_preview_factors;         // The list of default preview factors for each filter.
CImgList<unsigned int> gmic_button_parameters; // The list of button parameters for the current filter.
CImg<gmic_pixel_type> computed_preview;        // The last computed preview image.
CImg<unsigned char> latest_preview_buffer;     // The latest content of the preview buffer.
CImg<char> gmic_additional_commands;           // The buffer of additional G'MIC command implementations.
bool _create_dialog_gui;                       // Return value for 'create_gui_dialog()' (set by events handlers).
bool is_block_preview = false;                 // Flag to block preview, when double-clicking on the filter tree.
void **event_infos;                            // Infos that are passed to the GUI callback functions.
int image_id = 0;                              // The image concerned by the plug-in execution.
int preview_image_id = 0;                      // The alternate preview image used if image is too small.
int gimp_preview_width = 0;                    // The width of the last preview widget created.
int gimp_preview_height = 0;                   // The height of the last preview widget created.
double preview_image_factor = 0;               // If alternative preview image used, tell about the size factor (>1).
unsigned int indice_faves = 0;                 // The starting index of favorite filters.
unsigned int nb_available_filters = 0;         // The number of available filters (non-testing).
std::FILE *logfile = 0;                        // The log file if any.
void *p_spt = 0;                               // A pointer to the current running G'MIC thread if any (or 0).
GimpRunMode run_mode;                          // Run-mode used to call the plug-in.
GtkTreeStore *tree_view_store = 0;             // The list of the filters as a GtkTreeView model.
GtkWidget *dialog_window = 0;                  // The plug-in dialog window.
GtkWidget *gui_preview = 0;                    // The preview window.
GtkWidget *gui_preview_warning = 0;            // Warning label displaying for unaccurate preview.
GtkWidget *gui_preview_align = 0;              // Alignment widget that contains the preview.
GtkWidget *relabel_hbox = 0;                   // The entire widget to relabel filter.
GtkWidget *relabel_entry = 0;                  // The text entry to relabel filter.
GtkWidget *tree_view = 0;                      // The filter treeview.
GtkWidget *tree_mode_stock = 0;                // A temporary stock button for the expand/collapse button.
GtkWidget *tree_mode_button = 0;               // Expand/Collapse button for the treeview.
GtkWidget *refresh_stock = 0;                  // A temporary stock button for the refresh button.
GtkWidget *reset_zoom_stock = 0;               // A temporary stock button for the reset zoom button.
GtkWidget *fave_stock = 0;                     // A temporary stock button for the fave button.
GtkWidget *delete_stock = 0;                   // A temporary stock button for the fave button 2.
GtkWidget *fave_add_button = 0;                // Fave button.
GtkWidget *fave_delete_button = 0;             // Fave delete button.
GtkWidget *right_frame = 0;                    // The right frame containing the filter parameters.
GtkWidget *right_pane = 0;                     // The right scrolled window, containing the right frame.
GtkWidget *markup2ascii = 0;                   // Used to convert markup to ascii strings.
GimpPDBStatusType status = GIMP_PDB_SUCCESS;   // The plug-in return status.
#if GIMP_MINOR_VERSION>8
GimpColorProfile* img_profile = 0;             // The color profile of the image.
#endif

const char *s_blendmode[] = { "alpha","dissolve","behind","multiply","screen","overlay","difference",
                              "add","subtract","darken","lighten","hue","saturation","color","value",
                              "divide","dodge","burn","hardlight","softlight","grainextract",
                              "grainmerge","colorerase" };


// Set/get plug-in persistent variables, using GIMP {get,set}_data() features.
//-----------------------------------------------------------------------------

// Set/get the number of available filters.
void set_nbfilters(const unsigned int nb_filters) {
  gimp_set_data("gmic_nbfilters",&nb_filters,sizeof(unsigned int));
}

unsigned int get_nbfilters() {
  unsigned int nb_filters = 0;
  gimp_get_data("gmic_nbfilters",&nb_filters);
  return nb_filters;
}

// Set/get the indice of the currently selected filter.
void set_current_filter(const unsigned int current_filter) {
  const unsigned int ncurrent_filter = current_filter>=gmic_entries.size()?0:current_filter;
  gimp_set_data("gmic_current_filter",&ncurrent_filter,sizeof(unsigned int));
}

unsigned int get_current_filter() {
  unsigned int current_filter = 0;
  gimp_get_data("gmic_current_filter",&current_filter);
  if (current_filter>=gmic_entries.size()) current_filter = 0;
  return current_filter;
}

// Set/get the number of parameters of the specified filter.
void set_filter_nbparams(const unsigned int filter, const unsigned int nbparams) {
  CImg<char> s_tmp(48);
  cimg_snprintf(s_tmp,s_tmp.width(),"gmic_filter%u_nbparams",filter);
  gimp_set_data(s_tmp,&nbparams,sizeof(unsigned int));
}

unsigned int get_filter_nbparams(const unsigned int filter) {
  CImg<char> s_tmp(48);
  cimg_snprintf(s_tmp,s_tmp.width(),"gmic_filter%u_nbparams",filter);
  unsigned int nbparams = 0;
  gimp_get_data(s_tmp,&nbparams);
  return nbparams;
}

// Set/get one particular parameter of a filter.
void set_filter_parameter(const unsigned int filter, const unsigned int n, const char *const param) {
  CImg<char> s_tmp(48);
  cimg_snprintf(s_tmp,s_tmp.width(),"gmic_filter%u_parameter%u",filter,n);
  gimp_set_data(s_tmp,param,std::strlen(param) + 1);
}

CImg<char> get_filter_parameter(const unsigned int filter, const unsigned int n) {
  CImg<char> s_param, s_tmp(48);
  cimg_snprintf(s_tmp,s_tmp.width(),"gmic_filter%u_parameter%u",filter,n);
  const unsigned int siz = 1U + gimp_get_data_size(s_tmp);
  s_param.assign(siz);
  *s_param = 0;
  gimp_get_data(s_tmp,s_param);
  return s_param;
}

// Set/get one particular default parameter of a fave filter.
void set_fave_parameter(const unsigned int filter, const unsigned int n, const char *const param) {
  CImg<char> s_tmp(48);
  cimg_snprintf(s_tmp,s_tmp.width(),"gmic_fave%u_parameter%u",filter,n);
  gimp_set_data(s_tmp,param,std::strlen(param) + 1);
}

CImg<char> get_fave_parameter(const unsigned int filter, const unsigned int n) {
  CImg<char> s_param, s_tmp(48);
  cimg_snprintf(s_tmp,s_tmp.width(),"gmic_fave%u_parameter%u",filter,n);
  const unsigned int siz = 1U + gimp_get_data_size(s_tmp);
  s_param.assign(siz);
  *s_param = 0;
  gimp_get_data(s_tmp,s_param);
  return s_param;
}

// Reset all parameters of all filters.
void reset_filters_parameters() {
  const char *const empty = "";
  for (unsigned int i = 1; i<gmic_entries.size(); ++i)
    for (unsigned int j = 0; ; ++j) {
      const CImg<char> val = get_filter_parameter(i,j);
      if (*val) set_filter_parameter(i,j,empty); else break;
    }
}

// Set/get the filter input, output, preview, verbosity modes and log file.
void set_input_mode(const unsigned int input_mode) {
  gimp_set_data("gmic_input_mode",&input_mode,sizeof(unsigned int));
}

unsigned int get_input_mode(const bool normalized=true) {
  unsigned int input_mode = 0;
  gimp_get_data("gmic_input_mode",&input_mode);
  return normalized?(input_mode<2?1:(input_mode - 2)):input_mode;
}

void set_output_mode(const unsigned int output_mode) {
  gimp_set_data("gmic_output_mode",&output_mode,sizeof(unsigned int));
}

unsigned int get_output_mode(const bool normalized=true) {
  unsigned int output_mode = 0;
  gimp_get_data("gmic_output_mode",&output_mode);
  return normalized?(output_mode<2?0:(output_mode - 2)):output_mode;
}

void set_preview_mode(const unsigned int preview_mode) {
  gimp_set_data("gmic_preview_mode",&preview_mode,sizeof(unsigned int));
}

unsigned int get_preview_mode(const bool normalized=true) {
  unsigned int preview_mode = 0;
  gimp_get_data("gmic_preview_mode",&preview_mode);
  return normalized?(preview_mode<2?0:(preview_mode - 2)):preview_mode;
}

void set_preview_size(const unsigned int preview_size) {
  gimp_set_data("gmic_preview_size",&preview_size,sizeof(unsigned int));
}

unsigned int get_preview_size(const bool normalized=true) {
  unsigned int preview_size = 0;
  gimp_get_data("gmic_preview_size",&preview_size);
  return normalized?(preview_size<2?0:(preview_size - 2)):preview_size;
}

void set_verbosity_mode(const unsigned int verbosity) {
  gimp_set_data("gmic_verbosity_mode",&verbosity,sizeof(unsigned int));
}

unsigned int get_verbosity_mode(const bool normalized=true) {
  unsigned int verbosity_mode = 0;
  gimp_get_data("gmic_verbosity_mode",&verbosity_mode);
  return normalized?(verbosity_mode<2?0:(verbosity_mode - 2)):verbosity_mode;
}

void set_logfile() {
  const unsigned int verbosity = get_verbosity_mode();
  if (verbosity==3 || verbosity==5 || verbosity==7) {
    if (!logfile) {
      CImg<char> filename(1024);
      cimg_snprintf(filename,filename.width(),"%sgimp_log",
                    gmic::path_rc());
      logfile = std::fopen(filename,"a");
    }
    cimg::output(logfile?logfile:stdout);
  } else {
    if (logfile) std::fclose(logfile);
    logfile = 0;
    cimg::output(stdout);
  }
}

// Set/get the tree collapse/expand mode.
void set_tree_mode(const bool expand) {
  gimp_set_data("gmic_tree_mode",&expand,sizeof(bool));
}

bool get_tree_mode() {
  bool tree_mode = false;
  gimp_get_data("gmic_tree_mode",&tree_mode);
  return tree_mode;
}

// Set/get the net update activation state.
void set_net_update(const bool net_update) {
  gimp_set_data("gmic_net_update",&net_update,sizeof(bool));
}

bool get_net_update() {
  bool net_update = true;
  gimp_get_data("gmic_net_update",&net_update);
  return net_update;
}

// Set/get the current locale.
void set_locale() {
  CImg<char> locale(16); *locale = 0;
  const char *s_locale = gimp_gimprc_query("language");
  if (!s_locale || std::strlen(s_locale)<2) s_locale = setlocale(LC_CTYPE,0);
  if (!s_locale || std::strlen(s_locale)<2 || !cimg::strncasecmp("lc",s_locale,2)) s_locale = getenv("LANG");
  if (!s_locale || std::strlen(s_locale)<2) s_locale = getenv("LANGUAGE");
  if (!s_locale || std::strlen(s_locale)<2) s_locale = getenv("LC_ALL");
  if (!s_locale || std::strlen(s_locale)<2) s_locale = getenv("LC_CTYPE");
  if (!s_locale || std::strlen(s_locale)<2) s_locale = getenv("LC_TIME");
  if (!s_locale || std::strlen(s_locale)<2) s_locale = getenv("LC_NAME");
  if (!s_locale || std::strlen(s_locale)<2) s_locale = "en";
  cimg_sscanf(s_locale,"%c%c",&(locale[0]),&(locale[1]));
  locale[2] = 0;
  cimg::lowercase(locale);
  gimp_set_data("gmic_locale",locale,std::strlen(locale) + 1);
}

CImg<char> get_locale() {
  CImg<char> locale(16);
  *locale = 0;
  gimp_get_data("gmic_locale",locale);
  return locale;
}

// Get layer blending mode from string.
//-------------------------------------
void get_output_layer_props(const char *const s, GimpLayerModeEffects &blendmode, double &opacity,
                            int &posx, int &posy, CImg<char>& name) {
#define _get_output_layer_blendmode(in,out) \
  if (S && !is_blendmode) { \
    const unsigned int l = (unsigned int)std::strlen(in); \
    if (!std::strncmp(S + 5,in,l) && S[5 + l]==')') { blendmode = out; is_blendmode = true; } \
  }

  if (!s || !*s) return;

  // Read output blending mode.
  const char *S = std::strstr(s,"mode(");
  bool is_blendmode = false;
  _get_output_layer_blendmode("alpha",GIMP_NORMAL_MODE);
  _get_output_layer_blendmode("normal",GIMP_NORMAL_MODE);
  _get_output_layer_blendmode("dissolve",GIMP_DISSOLVE_MODE);
  _get_output_layer_blendmode("lighten",GIMP_LIGHTEN_ONLY_MODE);
  _get_output_layer_blendmode("screen",GIMP_SCREEN_MODE);
  _get_output_layer_blendmode("dodge",GIMP_DODGE_MODE);
  _get_output_layer_blendmode("add",GIMP_ADDITION_MODE);
  _get_output_layer_blendmode("darken",GIMP_DARKEN_ONLY_MODE);
  _get_output_layer_blendmode("multiply",GIMP_MULTIPLY_MODE);
  _get_output_layer_blendmode("burn",GIMP_BURN_MODE);
  _get_output_layer_blendmode("overlay",GIMP_OVERLAY_MODE);
  _get_output_layer_blendmode("softlight",GIMP_SOFTLIGHT_MODE);
  _get_output_layer_blendmode("hardlight",GIMP_HARDLIGHT_MODE);
  _get_output_layer_blendmode("difference",GIMP_DIFFERENCE_MODE);
  _get_output_layer_blendmode("subtract",GIMP_SUBTRACT_MODE);
  _get_output_layer_blendmode("grainextract",GIMP_GRAIN_EXTRACT_MODE);
  _get_output_layer_blendmode("grainmerge",GIMP_GRAIN_MERGE_MODE);
  _get_output_layer_blendmode("divide",GIMP_DIVIDE_MODE);
  _get_output_layer_blendmode("hue",GIMP_HUE_MODE);
  _get_output_layer_blendmode("saturation",GIMP_SATURATION_MODE);
  _get_output_layer_blendmode("color",GIMP_COLOR_MODE);
  _get_output_layer_blendmode("value",GIMP_VALUE_MODE);

  // Read output opacity.
  double _opacity = 0;
  char sep = 0;
  S = std::strstr(s,"opacity(");
  if (S && cimg_sscanf(S + 8,"%lf%c",&_opacity,&sep)==2 && sep==')') opacity = _opacity;
  if (opacity<0) opacity = 0; else if (opacity>100) opacity = 100;

  // Read output positions.
  int _posx = 0, _posy = 0;
  sep = 0;
  S = std::strstr(s,"pos(");
  if (S && cimg_sscanf(S + 4,"%d%*c%d%c",&_posx,&_posy,&sep)==3 && sep==')') { posx = _posx; posy = _posy; }

  // Read output name.
  S = std::strstr(s,"name(");
  if (S) {
    const char *ps = S + 5;
    unsigned int level = 1;
    while (*ps && level) {
      if (*ps=='(') ++level;
      else if (*ps==')') --level;
      ++ps;
    }
    if (!level || *(ps - 1)==')') {
      name.assign(S + 5,(unsigned int)(ps - S - 5)).back() = 0;
      cimg_for(name,pn,char) if (*pn==21) *pn = '('; else if (*pn==22) *pn = ')';
    }
  }
}

// Translate string into the current locale.
//------------------------------------------
#define _t(source,dest) if (!std::strcmp(source,s)) { const char *const ns = dest; return ns; }
const char *t(const char *const s) {
  const CImg<char> locale = get_locale();

  // Catalan translation.
  if (!std::strcmp(locale,"ca")) {
    if (!s) {
      const char *const ns = "No ha estat possible establir una connexi&#243; a Internet !\n\n"
        "No es possible arribar a aquestes fonts de filtres :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Av\303\255s:</b> Previsualitzaci\303\263 pot ser inexacta\n"
       "(factor de zoom s'ha modificat)</span>");
    _t("G'MIC for GIMP","G'MIC per al GIMP");
    _t("<i>Select a filter...</i>","<i>Selecciona un filtre...</i>");
    _t("<i>No parameters to set...</i>","<i>Sense par\303\240metres...</i>");
    _t("<b> Input / Output </b>","<b> Entrades / Sortides </b>");
    _t("Input layers...","Capes d'entrada...");
    _t("None","Cap");
    _t("Active (default)","Actiu (predet.)");
    _t("All","Tots");
    _t("Active & below","L'activa i les de sota");
    _t("Active & above","L'activa i les de sobre");
    _t("All visibles","Totes les visibles");
    _t("All invisibles","Totes les invisibles");
    _t("All visibles (decr.)","Totes les visibles (decr.)");
    _t("All invisibles (decr.)","Totes les invisibles (decr.)");
    _t("All (decr.)","Totes (decr.)");
    _t("Output mode...","Mode de sortida...");
    _t("In place (default)","A la capa actual (predet.)");
    _t("New layer(s)","Nova/es capa/es");
    _t("New active layer(s)","Nova/es capa/es actius");
    _t("New image","Nova imatge");
    _t("Preview mode...","Previsualitzaci\303\263 de sortida...");
    _t("1st output (default)","1era imatge (predet.)");
    _t("2nd output","2ona imatge");
    _t("3rd output","3era imatge");
    _t("4th output","4rta imatge");
    _t("1st -> 2nd","1era -> 2ona");
    _t("1st -> 3rd","1era -> 3era");
    _t("1st -> 4th","1era -> 4rta");
    _t("All outputs","Totes les imatges");
    _t("Output messages...","Missatges de sortida...");
    _t("Quiet (default)","Sense missatges (predet.)");
    _t("Verbose (layer name)","Verb\303\263s (nom de la capa)");
    _t("Verbose (console)","Verb\303\263s (consola)");
    _t("Verbose (logfile)","Verb\303\263s (arxiu)");
    _t("Very verbose (console)","Molt verb\303\263s (consola)");
    _t("Very verbose (logfile)","Molt verb\303\263s (arxiu)");
    _t("Debug mode (console)","Depuraci\303\263 (consola)");
    _t("Debug mode (logfile)","Depuraci\303\263 (arxiu)");
    _t("Preview size...","Mida de previsualitzaci\303\263...");
    _t("Tiny","Molt petita");
    _t("Small","Petita");
    _t("Normal","Normal");
    _t("Large","Gran");
    _t(" Available filters (%u)"," Filtres disponibles (%u)");
    _t("Update","Actualitzaci\303\263");
    _t("Rename","Canviar");
    _t("Add filter to faves","Afegir filtre a favorits");
    _t("Remove filter from faves","Remogui el filtre de favorits");
    _t("Update filters","filtres Actualitzar");
    _t("Enable Internet updates","Habilitar actualitzacions d'Internet");
    _t("Expand/collapse","Expand/collapse");
    _t("Reset zoom","Restablir zoom");
  }

  // Dutch translation.
  else if (!std::strcmp(locale,"nl")) {
    if (!s) {
      const char *const ns = "Geen internet-update mogelijk !\n\n"
        "Kan deze filters bronnen te bereiken :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Waarschuwing:</b> Voorbeeld mogelijk niet correct\n"
       "(zoomratio veranderd)﻿</span>");
    _t("G'MIC for GIMP","G'MIC voor GIMP");
    _t("<i>Select a filter...</i>","<i>Kies een filter...</i>");
    _t("<i>No parameters to set...</i>","<i>Geen parameters nodig...</i>");
    _t("<b> Input / Output </b>","<b> Input / Output </b>");
    _t("Input layers...","Input lagen...");
    _t("None","Geen");
    _t("Active (default)","Actieve laag (standaard)");
    _t("All","Alle");
    _t("Active & below","Actieve & onderliggende");
    _t("Active & above","Actieve & bovenliggende");
    _t("All visibles","Alle zichtbare");
    _t("All invisibles","Alle niet zichtbare");
    _t("All visibles (decr.)","Alle zichtbare (afnemend)");
    _t("All invisibles (decr.)","Alle niet zichtbare (afnemend)");
    _t("All (decr.)","Alle (afnemend)");
    _t("Output mode...","Output mode...");
    _t("In place (default)","Vervang bestaande (standaard)");
    _t("New layer(s)","Nieuwe laag/lagen");
    _t("New active layer(s)","Nieuwe actieve laag/lagen");
    _t("New image","Nieuwe afbeelding");
    _t("Preview mode...","Output voorbeeld...");
    _t("1st output (default)","1e Resultaat (standaard)");
    _t("2nd output","2e Resultaat");
    _t("3rd output","3e Resultaat");
    _t("4th output","4e Resultaat");
    _t("1st -> 2nd","1e -> 2e");
    _t("1st -> 3rd","1e -> 3e");
    _t("1st -> 4th","1e -> 4e");
    _t("All outputs","Alle resultaten");
    _t("Output messages...","Output berichten...");
    _t("Quiet (default)","Geen melding (standaard)");
    _t("Verbose (layer name)","Uitgebreid (laag naam)");
    _t("Verbose (console)","Uitgebreid (console)");
    _t("Verbose (logfile)","Uitgebreid (logfile)");
    _t("Very verbose (console)","Heel uitgebreid (console)");
    _t("Very verbose (logfile)","Heel uitgebreid (logfile)");
    _t("Debug mode (console)","Debug mode (console)");
    _t("Debug mode (logfile)","Debug mode (logfile)");
    _t("Preview size...","Voorbeeldformaat...");
    _t("Tiny","Zeer klein");
    _t("Small","Klein");
    _t("Normal","Normale");
    _t("Large","Groot");
    _t(" Available filters (%u)"," Beschikbare filters (%u)");
    _t("Update","Ververs");
    _t("Rename","Hernoemen");
    _t("Add filter to faves","Filter toevoegen aan favorieten");
    _t("Remove filter from faves","Verwijder filter uit favorieten");
    _t("Update filters","Update filters");
    _t("Enable Internet updates","Enable Internet updates");
    _t("Expand/collapse","Expand/collapse");
    _t("Reset zoom","Reset zoom");
  }

  // French translation.
  else if (!std::strcmp(locale,"fr")) {
    if (!s) {
      const char *const ns = "Mise &#224; jour depuis Internet incompl&#232;te !\n\n"
        "Acc&#232;s impossible aux sources de filtres :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Avertissement:</b> L'aper\303\247u est probablement inexact\n"
       "(le facteur de zoom a \303\251t\303\251 modifi\303\251)</span>");
    _t("G'MIC for GIMP","G'MIC pour GIMP");
    _t("<i>Select a filter...</i>","<i>Choisissez un filtre...</i>");
    _t("<i>No parameters to set...</i>","<i>Pas de param&#232;tres...</i>");
    _t("<b> Input / Output </b>","<b> Entr&#233;es / Sorties </b>");
    _t("Input layers...","Calques d'entr\303\251e...");
    _t("None","Aucun");
    _t("Active (default)","Actif (d\303\251faut)");
    _t("All","Tous");
    _t("Active & below","Actif & en dessous");
    _t("Active & above","Actif & au dessus");
    _t("All visibles","Tous les visibles");
    _t("All invisibles","Tous les invisibles");
    _t("All visibles (decr.)","Tous les visibles (d\303\251cr.)");
    _t("All invisibles (decr.)","Tous les invisibles (d\303\251cr.)");
    _t("All (decr.)","Tous (d\303\251cr.)");
    _t("Output mode...","Mode de sortie...");
    _t("In place (default)","Sur place (d\303\251faut)");
    _t("New layer(s)","Nouveau(x) calque(s)");
    _t("New active layer(s)","Nouveau(x) calque(s) actifs");
    _t("New image","Nouvelle image");
    _t("Preview mode...","Mode d'aper\303\247u...");
    _t("1st output (default)","1\303\250re image (d\303\251faut)");
    _t("2nd output","2\303\250me image");
    _t("3rd output","3\303\250me image");
    _t("4th output","4\303\250me image");
    _t("1st -> 2nd","1\303\250re -> 2\303\250me");
    _t("1st -> 3rd","1\303\250re -> 3\303\250me");
    _t("1st -> 4th","1\303\250re -> 4\303\250me");
    _t("All outputs","Toutes les images");
    _t("Output messages...","Messages de sortie...");
    _t("Quiet (default)","Aucun message (d\303\251faut)");
    _t("Verbose (layer name)","Mode verbeux (nom de calque)");
    _t("Verbose (console)","Mode verbeux (console)");
    _t("Verbose (logfile)","Mode verbeux (fichier log)");
    _t("Very verbose (console)","Mode tr\303\250s verbeux (console)");
    _t("Very verbose (logfile)","Mode tr\303\250s verbeux (fichier log)");
    _t("Debug mode (console)","Mode d\303\251bogage (console)");
    _t("Debug mode (logfile)","Mode d\303\251bogage (fichier log)");
    _t("Preview size...","Taille d'aper\303\247u...");
    _t("Tiny","Minuscule");
    _t("Small","Petit");
    _t("Normal","Normal");
    _t("Large","Grand");
    _t(" Available filters (%u)"," Filtres disponibles (%u)");
    _t("Update","Actualiser");
    _t("Rename","Renommer");
    _t("Add filter to faves","Ajouter le filtre aux favoris");
    _t("Remove filter from faves","Retirer le filtre des favoris");
    _t("Update filters","Actualiser les filtres");
    _t("Enable Internet updates","Autoriser les mises \303\240 jour Internet");
    _t("Expand/collapse","D\303\251plier/Replier");
    _t("Reset zoom","R\303\251initialize le zoom");
  }

  // German translation.
  else if (!std::strcmp(locale,"de")) {
    if (!s) {
      const char *const ns = "Kein Internet-Update m\303\266glich !\n\n"
        "Kann diese Filter Quellen erreichen :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Warnung:</b> Vorschau kann ungenau sein\n"
       "(Vergr\303\266\303\237erung wurde ver\303\244ndert)﻿</span>");
    _t("G'MIC for GIMP","G'MIC f\303\274r GIMP");
    _t("<i>Select a filter...</i>","<i>W\303\244hlen Sie einen Filter...</i>");
    _t("<i>No parameters to set...</i>","<i>Keine w\303\244hlbaren Parameter...</i>");
    _t("<b> Input / Output </b>","<b> Eingabe / Ausgabe </b>");
    _t("Input layers...","Eingabeebenen...");
    _t("None","Keine");
    _t("Active (default)","Aktive (Standard)");
    _t("All","Alle");
    _t("Active & below","Aktive & darunterliegende");
    _t("Active & above","Aktive & dar\303\274berliegende");
    _t("All visibles","Alle sichtbaren");
    _t("All invisibles","Alle nicht sichtbaren");
    _t("All visibles (decr.)","Alle sichtbaren (absteigend)");
    _t("All invisibles (decr.)","Alle nicht sichtbaren (absteigend)");
    _t("All (decr.)","Alle (absteigend)");
    _t("Output mode...","Ausgabemodus...");
    _t("In place (default)","Bestehende ersetzen (standard)");
    _t("New layer(s)","Neue Ebene(n)");
    _t("New active layer(s)","Neue aktive Ebene(n)");
    _t("New image","Neues Bild");
    _t("Preview mode...","Ausgabevorschau...");
    _t("1st output (default)","1. Ausgabe (Standard)");
    _t("2nd output","2. Ausgabe");
    _t("3rd output","3. Ausgabe");
    _t("4th output","4. Ausgabe");
    _t("1st -> 2nd","1. -> 2.");
    _t("1st -> 3rd","1. -> 3.");
    _t("1st -> 4th","1. -> 4.");
    _t("All outputs","Alle Ausgaben");
    _t("Output messages...","Ausgabemeldungen...");
    _t("Quiet (default)","Keine Meldung (Standard)");
    _t("Verbose (layer name)","Ausf\303\274hrlich (Ebenennamen)");
    _t("Verbose (console)","Ausf\303\274hrlich (Konsole)");
    _t("Verbose (logfile)","Ausf\303\274hrlich (Logfile)");
    _t("Very verbose (console)","Sehr ausf\303\274hrlich (Konsole)");
    _t("Very verbose (logfile)","Sehr ausf\303\274hrlich (Logfile)");
    _t("Debug mode (console)","Debug-Modus (Konsole)");
    _t("Debug mode (logfile)","Debug-Modus (Logfile)");
    _t("Preview size...","Vorschaugrosse...");
    _t("Tiny","Winzig");
    _t("Small","Klein");
    _t("Normal","Normal");
    _t("Large","Gross");
    _t(" Available filters (%u)"," Verf\303\274gbare Filter (%u)");
    _t("Update","Update");
    _t("Rename","Umbenennen");
    _t("Add filter to faves","Filter zu Favoriten hinzuf\303\274gen");
    _t("Remove filter from faves","Filter aus Favoriten entfernen");
    _t("Update filters","Filter updaten");
    _t("Enable Internet updates","Internet-Updates aktivieren");
    _t("Expand/collapse","Ein-/Ausklappen﻿");
    _t("Reset zoom","Reset Zoom");
  }

  // Italian translation.
  else if (!std::strcmp(locale,"it")) {
    if (!s) {
      const char *const ns = "Impossibile aggiornare da Internet !\n\n"
        "Impossibile raggiungere queste fonti filtri :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Attenzione:</b> L'anteprima pu\303\262 essere inaccurata\n"
       "(il fattore di zoom \303\250 stato modificato)</span>");
    _t("G'MIC for GIMP","G'MIC per GIMP");
    _t("<i>Select a filter...</i>","<i>Sciegliete un Filtro...</i>");
    _t("<i>No parameters to set...</i>","<i>Filtro senza Parametri...</i>");
    _t("<b> Input / Output </b>","<b> Input / Output </b>");
    _t("Input layers...","Input da Layers...");
    _t("None","Nessuno");
    _t("Active (default)","Layer Attivo (default)");
    _t("All","Tutti");
    _t("Active & below","Attivo & superiori");
    _t("Active & above","Attivo & inferiori");
    _t("All visibles","Tutti i Visibili");
    _t("All invisibles","Tutti gli invisibili");
    _t("All visibles (decr.)","Tutti i visibili (dal fondo)");
    _t("All invisibles (decr.)","Tutti gli invisibili (dal fondo)");
    _t("All (decr.)","Tutti (dal fondo)");
    _t("Output mode...","Tipo di output...");
    _t("In place (default)","Applica al Layer attivo (default) ");
    _t("New layer(s)","Nuovo(i) Layer(s)");
    _t("New active layer(s)","Attiva Nuovo(i) Layer(s)");
    _t("New image","Nuova Immagine");
    _t("Preview mode...","Anteprima...");
    _t("1st output (default)","Primo Output (default)");
    _t("2nd output","Secondo Output");
    _t("3rd output","Terzo Output");
    _t("4th output","Quarto Output");
    _t("1st -> 2nd","1 -> 2");
    _t("1st -> 3rd","1 -> 3");
    _t("1st -> 4th","1 -> 4");
    _t("All outputs","Tutti i layers");
    _t("Output messages...","Messaggi di Output...");
    _t("Quiet (default)","Nessun Messaggio (default)");
    _t("Verbose (layer name)","Messagi (nome del Layer)");
    _t("Verbose (console)","Messagi (console)");
    _t("Verbose (logfile)","Messagi (logfile)");
    _t("Very verbose (console)","Messaggi Dettagliati (console)");
    _t("Very verbose (logfile)","Messaggi Dettagliati (logfile)");
    _t("Debug mode (console)","Debug Mode (console)");
    _t("Debug mode (logfile)","Debug Mode (logfile)");
    _t("Preview size...","Anteprima dimensioni...");
    _t("Tiny","Minuscolo");
    _t("Small","Piccolo");
    _t("Normal","Normale");
    _t("Large","Grande");
    _t(" Available filters (%u)"," Filtri disponibili (%u)");
    _t("Update","Aggiornare");
    _t("Rename","Rinominare");
    _t("Add filter to faves","Aggiungi filtro ai favoriti");
    _t("Remove filter from faves","Rimuovi filtro dai favoriti");
    _t("Update filters","Aggiorna filtri");
    _t("Enable Internet updates","Abilita aggiornamenti Internet");
    _t("Expand/collapse","Espandi/comprimi");
    _t("Reset zoom","Reimpostare lo zoom");
  }

  // Japanese translation.
  else if (!std::strcmp(locale,"ja")) {
    if (!s) {
      const char *const ns = "\xE3\x82\xA4\xE3\x83\xB3\xE3\x82\xBF\xE3\x83\xBC\xE3\x83\x8D\xE3\x83\x83\xE3\x83"
        "\x88\xE7\xB5\x8C\xE7\x94\xB1\xE3\x81\xA7\xE3\x81\xAE\xE6\x9B\xB4\xE6\x96\xB0\xE3\x81\xAB\xE5\xA4\xB1\xE6\x95"
        "\x97\xE3\x81\x97\xE3\x81\x9F\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x81\x8C\xE3\x81\x82\xE3\x82"
        "\x8A\xE3\x81\xBE\xE3\x81\x99\xEF\xBC\x81\n\n"
        "\xE4\xBB\xA5\xE4\xB8\x8B\xE3\x81\xAE\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x81\xAE\xE9\x85\x8D"
        "\xE5\xB8\x83\xE5\x85\x83\xE3\x81\xAB\xE3\x82\xA2\xE3\x82\xAF\xE3\x82\xBB\xE3\x82\xB9\xE3\x81\xA7\xE3\x81\x8D"
        "\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x81\xA7\xE3\x81\x97\xE3\x81\x9F\xE3\x80\x82\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>\xE8\xAD\xA6\xE5\x91\x8A:</b> \xE3\x83\x97\xE3\x83\xAC\xE3\x83\x93\xE3\x83\xA5"
       "\xE3\x83\xBC\xE3\x81\xAF\xE5\xAE\x9F\xE9\x9A\x9B\xE3\x81\xAE\xE5\xAE\x9F\xE8\xA1\x8C\xE7\xB5\x90\xE6\x9E\x9C"
       "\xE3\x81\xA8\xE7\x95\xB0\xE3\x81\xAA\xE3\x82\x8B\xE5\xA0\xB4\xE5\x90\x88\xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A"
       "\xE3\x81\xBE\xE3\x81\x99\n"
       "(\xE6\x8B\xA1\xE5\xA4\xA7\xE7\x8E\x87\xE3\x81\x8C\xE5\xA4\x89\xE6\x9B\xB4\xE3\x81\x95\xE3\x82\x8C\xE3\x81\xBE"
       "\xE3\x81\x97\xE3\x81\x9F)</span>");
    _t("G'MIC for GIMP","G'MIC for GIMP");
    _t("<i>Select a filter...</i>","<i>\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x82\x92\xE9\x81\xB8\xE6\x8A"
       "\x9E\xE3\x81\x97\xE3\x81\xA6\xE3\x81\x8F\xE3\x81\xA0\xE3\x81\x95\xE3\x81\x84...</i>");
    _t("<i>No parameters to set...</i>","<i>\xE8\xA8\xAD\xE5\xAE\x9A\xE3\x81\x99\xE3\x82\x8B\xE3\x83\x91\xE3\x83\xA9"
       "\xE3\x83\xA1\xE3\x83\xBC\xE3\x82\xBF\xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93"
       "...</i>");
    _t("<b> Input / Output </b>","<b> \xE5\x85\xA5\xE5\x8A\x9B\x2F\xE5\x87\xBA\xE5\x8A\x9B </b>");
    _t("Input layers...","\xE5\x85\xA5\xE5\x8A\x9B\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC...");
    _t("None","\xE3\x81\xAA\xE3\x81\x97");
    _t("Active (default)","\xE3\x82\xA2\xE3\x82\xAF\xE3\x83\x86\xE3\x82\xA3\xE3\x83\x96\xE3\x81\xAA\xE3\x83\xAC\xE3\x82"
       "\xA4\xE3\x83\xA4\xE3\x83\xBC (\xE3\x83\x87\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xAB\xE3\x83\x88)");
    _t("All","\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6");
    _t("Active & below","\xE3\x82\xA2\xE3\x82\xAF\xE3\x83\x86\xE3\x82\xA3\xE3\x83\x96 & \xE4\xB8\x80\xE3\x81\xA4\xE4"
       "\xB8\x8A\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC");
    _t("Active & above","\xE3\x82\xA2\xE3\x82\xAF\xE3\x83\x86\xE3\x82\xA3\xE3\x83\x96 & \xE4\xB8\x80\xE3\x81\xA4\xE4"
       "\xB8\x8B\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC");
    _t("All visibles","\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE3\x81\xAE\xE5\x8F\xAF\xE8\xA6\x96\xE3\x83\xAC\xE3\x82"
       "\xA4\xE3\x83\xA4\xE3\x83\xBC");
    _t("All invisibles","\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE3\x81\xAE\xE4\xB8\x8D\xE5\x8F\xAF\xE8\xA6\x96\xE3\x83"
       "\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC");
    _t("All visibles (decr.)","\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE3\x81\xAE\xE5\x8F\xAF\xE8\xA6\x96\xE3\x83\xAC\xE3"
       "\x82\xA4\xE3\x83\xA4\xE3\x83\xBC (\xE9\x80\x86\xE9\xA0\x86)");
    _t("All invisibles (decr.)","\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE3\x81\xAE\xE4\xB8\x8D\xE5\x8F\xAF\xE8\xA6\x96"
       "\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC (\xE9\x80\x86\xE9\xA0\x86)");
    _t("All (decr.)","\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6 (\xE9\x80\x86\xE9\xA0\x86)");
    _t("Output mode...","\xE5\x87\xBA\xE5\x8A\x9B\xE3\x83\xA2\xE3\x83\xBC\xE3\x83\x89...");
    _t("In place (default)","\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC "
       "(\xE3\x83\x87\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xAB\xE3\x83\x88)");
    _t("New layer(s)","\xE6\x96\xB0\xE8\xA6\x8F\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC");
    _t("New active layer(s)","\xE3\x82\xA2\xE3\x82\xAF\xE3\x83\x86\xE3\x82\xA3\xE3\x83\x96\xE3\x81\xAA\xE6\x96\xB0\xE8"
       "\xA6\x8F\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC");
    _t("New image","\xE6\x96\xB0\xE8\xA6\x8F\xE7\x94\xBB\xE5\x83\x8F");
    _t("Preview mode...","\xE3\x83\x97\xE3\x83\xAC\xE3\x83\x93\xE3\x83\xA5\xE3\x83\xBC\xE3\x83\xA2\xE3\x83\xBC\xE3\x83"
       "\x89...");
    _t("1st output (default)","1 \xE6\x9E\x9A\xE7\x9B\xAE\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC"
       "\xE3\x81\xAB\xE9\x81\xA9\xE7\x94\xA8 (\xE3\x83\x87\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xAB\xE3\x83\x88)");
    _t("2nd output","2 \xE6\x9E\x9A\xE7\x9B\xAE\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE3\x81\xAB"
       "\xE9\x81\xA9\xE7\x94\xA8");
    _t("3rd output","3 \xE6\x9E\x9A\xE7\x9B\xAE\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE3\x81\xAB"
       "\xE9\x81\xA9\xE7\x94\xA8");
    _t("4th output","4 \xE6\x9E\x9A\xE7\x9B\xAE\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE3\x81\xAB"
       "\xE9\x81\xA9\xE7\x94\xA8");
    _t("1st -> 2nd","1 ~ 2 \xE6\x9E\x9A\xE7\x9B\xAE\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE3\x81"
       "\xAB\xE9\x81\xA9\xE7\x94\xA8");
    _t("1st -> 3rd","1 ~ 3 \xE6\x9E\x9A\xE7\x9B\xAE\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE3\x81"
       "\xAB\xE9\x81\xA9\xE7\x94\xA8");
    _t("1st -> 4th","1 ~ 4 \xE6\x9E\x9A\xE7\x9B\xAE\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE3\x81"
       "\xAB\xE9\x81\xA9\xE7\x94\xA8");
    _t("All outputs","\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC"
       "\xE3\x81\xAB\xE9\x81\xA9\xE7\x94\xA8");
    _t("Output messages...","\xE5\x87\xBA\xE5\x8A\x9B\xE3\x83\xA1\xE3\x83\x83\xE3\x82\xBB\xE3\x83\xBC\xE3\x82\xB8...");
    _t("Quiet (default)","\xE3\x81\xAA\xE3\x81\x97 (\xE3\x83\x87\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xAB\xE3\x83\x88)");
    _t("Verbose (layer name)","\xE6\x83\x85\xE5\xA0\xB1\xE3\x82\x92\xE5\x87\xBA\xE5\x8A\x9B "
       "(\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE5\x90\x8D)");
    _t("Verbose (console)","\xE6\x83\x85\xE5\xA0\xB1\xE3\x82\x92\xE5\x87\xBA\xE5\x8A\x9B "
       "(\xE3\x82\xB3\xE3\x83\xB3\xE3\x82\xBD\xE3\x83\xBC\xE3\x83\xAB)");
    _t("Verbose (logfile)","\xE6\x83\x85\xE5\xA0\xB1\xE3\x82\x92\xE5\x87\xBA\xE5\x8A\x9B "
       "(\xE3\x83\xAD\xE3\x82\xB0\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB)");
    _t("Very verbose (console)","\xE8\xA9\xB3\xE7\xB4\xB0\xE3\x81\xAA\xE6\x83\x85\xE5\xA0\xB1\xE3\x82\x92\xE5\x87\xBA"
       "\xE5\x8A\x9B (\xE3\x82\xB3\xE3\x83\xB3\xE3\x82\xBD\xE3\x83\xBC\xE3\x83\xAB)");
    _t("Very verbose (logfile)","\xE8\xA9\xB3\xE7\xB4\xB0\xE3\x81\xAA\xE6\x83\x85\xE5\xA0\xB1\xE3\x82\x92\xE5\x87\xBA"
       "\xE5\x8A\x9B (\xE3\x83\xAD\xE3\x82\xB0\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB)");
    _t("Debug mode (console)","\xE3\x83\x87\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0\xE3\x83\xA2\xE3\x83\xBC\xE3\x83\x89 "
       "(\xE3\x82\xB3\xE3\x83\xB3\xE3\x82\xBD\xE3\x83\xBC\xE3\x83\xAB)");
    _t("Debug mode (logfile)","\xE3\x83\x87\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0\xE3\x83\xA2\xE3\x83\xBC\xE3\x83\x89 "
       "(\xE3\x83\xAD\xE3\x82\xB0\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB)");
    _t("Preview size...","\xE3\x83\x97\xE3\x83\xAC\xE3\x83\x93\xE3\x83\xA5\xE3\x83\xBC\xE3\x82\xB5\xE3\x82\xA4\xE3\x82"
       "\xBA...");
    _t("Tiny","\xE6\xA5\xB5\xE5\xB0\x8F");
    _t("Small","\xE5\xB0\x8F");
    _t("Normal","\xE6\x99\xAE\xE9\x80\x9A");
    _t("Large","\xE5\xA4\xA7");
    _t(" Available filters (%u)"," \xE5\x88\xA9\xE7\x94\xA8\xE5\x8F\xAF\xE8\x83\xBD\xE3\x81\xAA\xE3\x83\x95\xE3\x82\xA3"
       "\xE3\x83\xAB\xE3\x82\xBF (%u)");
    _t("Update","\xE6\x9B\xB4\xE6\x96\xB0");
    _t("Rename","\xE5\x90\x8D\xE5\x89\x8D\xE3\x82\x92\xE5\xA4\x89\xE6\x9B\xB4");
    _t("Internet","\xE3\x82\xA4\xE3\x83\xB3\xE3\x82\xBF\xE3\x83\xBC\xE3\x83\x8D\xE3\x83\x83\xE3\x83\x88");
    _t("Add filter to faves","\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x82\x92\xE3\x81\x8A\xE6\xB0\x97\xE3"
       "\x81\xAB\xE5\x85\xA5\xE3\x82\x8A\xE3\x81\xAB\xE8\xBF\xBD\xE5\x8A\xA0");
    _t("Remove filter from faves","\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x82\x92\xE3\x81\x8A\xE6\xB0\x97"
       "\xE3\x81\xAB\xE5\x85\xA5\xE3\x82\x8A\xE3\x81\x8B\xE3\x82\x89\xE5\x89\x8A\xE9\x99\xA4");
    _t("Update filters","\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x82\x92\xE6\x9B\xB4\xE6\x96\xB0");
    _t("Enable Internet updates","\xE3\x82\xA4\xE3\x83\xB3\xE3\x82\xBF\xE3\x83\xBC\xE3\x83\x8D\xE3\x83\x83\xE3\x83\x88"
       "\xE7\xB5\x8C\xE7\x94\xB1\xE3\x81\xA7\xE3\x81\xAE\xE6\x9B\xB4\xE6\x96\xB0\xE3\x82\x92\xE6\x9C\x89\xE5\x8A\xB9"
       "\xE5\x8C\x96");
    _t("Expand/collapse","\xE5\xB1\x95\xE9\x96\x8B\x2F\xE6\x8A\x98\xE3\x82\x8A\xE3\x81\x9F\xE3\x81\x9F\xE3\x82\x80");
    _t("Reset zoom","\xE3\x82\xBA\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\x92\xE3\x83\xAA\xE3\x82\xBB\xE3\x83\x83\xE3\x83\x88");
  }

  // Polish translation.
  else if (!std::strcmp(locale,"pl")) {
    if (!s) {
      const char *const ns = "Aktualizacja filtr\303\263w przez internet (cz\304\231\305\233ciowo) nie "
        "powiod\305\202a si\304\231 !\n\n"
        "Brak dost\304\231pu do tych \305\272r\303\263de\305\202 filtr\303\263w :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Uwaga:</b> Podgl\304\205d mo\305\274e si\304\231 r\303\263\305\274ni\304\207 od "
       "efektu ko\305\204cowego\n"
       "ze wzgl\304\231du na zmian\304\231 przybli\305\274enia</span>");
    _t("G'MIC for GIMP","G'MIC dla GIMP");
    _t("<i>Select a filter...</i>","<i>Wybierz filtr...</i>");
    _t("<i>No parameters to set...</i>","<i>Brak parametr\304\205w do ustawienia...</i>");
    _t("<b> Input / Output </b>","<b> Wej\305\233cie / Wyj\305\233cie </b>");
    _t("Input layers...","Warstwy wej\305\233cia...");
    _t("None","Brak");
    _t("Active (default)","Aktywna (domy\305\233lnie)");
    _t("All","Wszystkie");
    _t("Active & below","Aktywna & poni\305\274ej");
    _t("Active & above","Aktywna & powy\305\274ej");
    _t("All visibles","Wszystkie widoczne");
    _t("All invisibles","Wszystkie niewidoczne");
    _t("All visibles (decr.)","Wszystkie widoczne (od do\305\202u)");
    _t("All invisibles (decr.)","Wszystkie niewidoczne (od do\305\202u)");
    _t("All (decr.)","Wszystkie (od do\305\202u)");
    _t("Output mode...","Tryb wyj\305\233cia...");
    _t("In place (default)","Na miejscu (domy\305\233lnie)");
    _t("New layer(s)","Nowa/e warstwa/y");
    _t("New active layer(s)","Nowa/e aktywna/e warstwa/y");
    _t("New image","Nowy obraz");
    _t("Preview mode...","Podgl\304\205d wyj\305\233cia dla warstw...");
    _t("1st output (default)","Pierwszej (domy\305\233lnie)");
    _t("2nd output","Drugiej");
    _t("3rd output","Trzeciej");
    _t("4th output","Czwartej");
    _t("1st -> 2nd","Od 1 do 2");
    _t("1st -> 3rd","Od 1 do 3");
    _t("1st -> 4th","Od 1 do 4");
    _t("All outputs","Wszystkich");
    _t("Output messages...","Komunikat wyj\305\233cia...");
    _t("Quiet (default)","Brak (domy\305\233lnie)");
    _t("Verbose (layer name)","Og\303\263lny (nazwa warstwy)");
    _t("Verbose (console)","Og\303\263lny (konsola)");
    _t("Verbose (logfile)","Og\303\263lny (plik log)");
    _t("Very verbose (console)","Dok\305\202adny (konsola)");
    _t("Very verbose (logfile)","Dok\305\202adny (plik log)");
    _t("Debug mode (console)","Debugowanie (konsola)");
    _t("Debug mode (logfile)","Debugowanie (plik log)");
    _t("Preview size...","Rozmiar podgl\304\205d...");
    _t("Tiny","Male");
    _t("Small","Mala");
    _t("Normal","Normalne");
    _t("Large","Duza");
    _t(" Available filters (%u)"," Dost\304\231pne filtry (%u)");
    _t("Update","Uaktualnij");
    _t("Rename","Zmiana nazwy");
    _t("Add filter to faves","Dodaj filtr do ulubionych");
    _t("Remove filter from faves","Usun filtr z Ulubionych");
    _t("Update filters","Filtry aktualizacji");
    _t("Enable Internet updates","Wlacz aktualizacje internetowe");
    _t("Expand/collapse","Rozwin/Zwin");
    _t("Reset zoom","Zresetowa\304\207 zoomu");
  }

  // Portuguese translation.
  else if (!std::strcmp(locale,"pt")) {
    if (!s) {
      const char *const ns = "A atualiza\303\247\303\243o pela internet falhou !\n\n"
        "Incapaz de chegar a essas fontes de filtros :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Aten\303\247\303\243o:</b> a pr\303\251-visualiza\303\247\303\243o pode "
       "estar incorreta\n"
       "(o fator amplia\303\247\303\243o foi modificado)</span>");
    _t("G'MIC for GIMP","G'MIC para o GIMP");
    _t("<i>Select a filter...</i>","<i>Escolha um filtro</i>");
    _t("<i>No parameters to set...</i>","<i>Sem par\303\242metros para configurar...</i>");
    _t("<b> Input / Output </b>","<b> Entrada / Saida </b>");
    _t("Input layers...","Camadas de Entrada...");
    _t("None","Nenhuma");
    _t("Active (default)","Ativo (Padr\303\243o)");
    _t("All","Todos");
    _t("Active & below","Ativo & abaixo");
    _t("Active & above","Ativo & acima");
    _t("All visibles","Todos vis\303\255veis");
    _t("All invisibles","Todos invis\303\255veis");
    _t("All visibles (decr.)","Todos vis\303\255veis (decr.)");
    _t("All invisibles (decr.)","Todos invis\303\255veis (decr.)");
    _t("All (decr.)","Todos (decr.)");
    _t("Output mode...","Modo de saida...");
    _t("In place (default)","No lugar (Padr\303\243o)");
    _t("New layer(s)","Nova(s) camada(s)");
    _t("New active layer(s)","Nova(s) camadas(s) ativa");
    _t("New image","Nova imagem");
    _t("Preview mode...","Pr\303\251 Visualiza\303\247\303\243o");
    _t("1st output (default)","Primeira pr\303\251via (Padr\303\243o)");
    _t("2nd output","2 pr\303\251via imagem");
    _t("3rd output","3 pr\303\251via imagem");
    _t("4th output","4 pr\303\251via imagem");
    _t("1st -> 2nd","1st -> 2nd");
    _t("1st -> 3rd","1st -> 3rd");
    _t("1st -> 4th","1st -> 4th");
    _t("All outputs","Todas as imagens");
    _t("Output messages...","Mensagens de saida...");
    _t("Quiet (default)","Quieto (Padr\303\243o)");
    _t("Verbose (layer name)","Mode verbose (nome da camada)");
    _t("Verbose (console)","Mode verbose (console)");
    _t("Verbose (logfile)","Mode verbose (arquivo)");
    _t("Very verbose (console)","Modo verbose ampliada (console)");
    _t("Very verbose (logfile)","Modo verbose ampliada (arquivo)");
    _t("Debug mode (console)","Modo Debug (console)");
    _t("Debug mode (logfile)","Modo Debug (arquivo)");
    _t("Preview size...","O tamanho da visualiza\303\247\303\243o...");
    _t("Tiny","Minusculo");
    _t("Small","Pequeno");
    _t("Normal","Normal");
    _t("Large","Grande");
    _t(" Available filters (%u)"," Filtros dispon\303\255veis (%u)");
    _t("Update","Atualizar");
    _t("Rename","Renomear");
    _t("Add filter to faves","Adicionar a favoritos filtro");
    _t("Remove filter from faves","Remover filtro de favoritos");
    _t("Update filters","Filtros de atualizacao");
    _t("Enable Internet updates","Ativar atualizacoes Internet");
    _t("Expand/collapse","Expandir/recolher");
    _t("Reset zoom","Redefinir zoom");
  }

  // Serbian translation.
  else if (!std::strcmp(locale,"sr")) {
    if (!s) {
      const char *const ns = "A\305\276uriranje filtera sa interneta (delimi\304\215no) neuspe\305\241no !\n\n"
        "Nije mogu\304\207e dospeti do izvorne lokacije ovih filtera :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Upozorenje :</b> Pregled mo\305\276e biti neta\304\215na\n"
       "( zum faktor je modifikovan )</span>");
    _t("G'MIC for GIMP","G'MIC za GIMP");
    _t("<i>Select a filter...</i>","<i>Izaberite filter...</i>");
    _t("<i>No parameters to set...</i>","<i>Nema parametara za pode\305\241avanje...</i>");
    _t("<b> Input / Output </b>","<b> Ulazni podaci / Rezultati </b>");
    _t("Input layers...","Ulazni slojevi...");
    _t("None","Nijedan");
    _t("Active (default)","Aktivan (podrazumevana opcija)");
    _t("All","Svi");
    _t("Active & below","Aktivni & ispod");
    _t("Active & above","Aktivni & iznad");
    _t("All visibles","Svi vidljivi");
    _t("All invisibles","Svi nevidljivi");
    _t("All visibles (decr.)","Svi vidljivi (po opadaju\304\207em nizu)");
    _t("All invisibles (decr.)","Svi nevidljivi (po opadaju\304\207em nizu)");
    _t("All (decr.)","Svi (po opadaju\304\207em nizu)");
    _t("Output mode...","Izlazni mod...");
    _t("In place (default)","Umesto (podrazumevana opcija)");
    _t("New layer(s)","Novi sloj(evi)");
    _t("New active layer(s)","Novi aktivni sloj(evi)");
    _t("New image","Nova slika");
    _t("Preview mode...","Pregled rezultata...");
    _t("1st output (default)","prvi rezultat (podrazumevana opcija)");
    _t("2nd output","drugi rezultat");
    _t("3rd output","tre\304\207i rezultat");
    _t("4th output","\304\215etvrti rezultat");
    _t("1st -> 2nd","prvi -> drugi");
    _t("1st -> 3rd","prvi -> tre\304\207i");
    _t("1st -> 4th","prvi -> \304\215etvrti");
    _t("All outputs","Svi rezultati");
    _t("Output messages...","Izlazne poruke...");
    _t("Quiet (default)","Tiho (podrazumevana opcija)");
    _t("Verbose (layer name)","Op\305\241irnije (slojevi)");
    _t("Verbose (console)","Op\305\241irnije (konzola)");
    _t("Verbose (logfile)","Op\305\241irnije (log fajl)");
    _t("Very verbose (console)","Vrlo op\305\241irno (konzola)");
    _t("Very verbose (logfile)","Vrlo op\305\241irno (log fajl)");
    _t("Debug mode (console)","Mod za otklanjanje programskih gre\305\241aka (konzola)");
    _t("Debug mode (logfile)","Mod za otklanjanje programskih gre\305\241aka (log fajl)");
    _t("Preview size...","Pregled velicine...");
    _t("Tiny","Veoma mali");
    _t("Small","Mali");
    _t("Normal","Normalno");
    _t("Large","Veliki");
    _t(" Available filters (%u)"," Raspolo\305\276ivi filteri (%u)");
    _t("Update","Azuriranje");
    _t("Rename","Preimenovati");
    _t("Add filter to faves","Dodaj filter to faves");
    _t("Remove filter from faves","Izvadite filter iz faves");
    _t("Update filters","Update filteri");
    _t("Enable Internet updates","Omoguci Internet ispravke");
    _t("Expand/collapse","Razvij/skupi");
    _t("Reset zoom","Reset zum");
  }

  // Spanish translation (Castillan).
  else if (!std::strcmp(locale,"es")) {
    if (!s) {
      const char *const ns = "No es posible establecer conexión a Internet !\n\n"
        "No es posible acceder a estas fuentes de filtros :\n";
      return ns;
    }
    _t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
       "(zoom factor has been modified)</span>",
       "\n<span color=\"#AA0000\"><b>Advertencia:</b> La vista previa puede ser inexacta\n"
       "(el factor de zoom ha sido modificado)</span>");
    _t("G'MIC for GIMP","G'MIC para GIMP");
    _t("<i>Select a filter...</i>","<i>Selecciona un filtro...</i>");
    _t("<i>No parameters to set...</i>","<i>Sin par\303\241metros...</i>");
    _t("<b> Input / Output </b>","<b> Entrada / Salida </b>");
    _t("Input layers...","Capas de entrada...");
    _t("None","Ninguna");
    _t("Active (default)","Activa (predet.)");
    _t("All","Todas");
    _t("Active & below","Activa e inferior");
    _t("Active & above","Activa y superior");
    _t("All visibles","Todas las visibles");
    _t("All invisibles","Todas las invisibles");
    _t("All visibles (decr.)","Todas las visibles (decr.)");
    _t("All invisibles (decr.)","Todas las invisibles (decr.)");
    _t("All (decr.)","Todas (decr.)");
    _t("Output mode...","Modo de salida...");
    _t("In place (default)","En la capa actual (predet.)");
    _t("New layer(s)","Capa/as nueva/as");
    _t("New active layer(s)","Capa/as nueva/as activa");
    _t("New image","Imagen nueva");
    _t("Preview mode...","Previsualizaci\303\263n de la salida...");
    _t("1st output (default)","1ra imagen (predet.)");
    _t("2nd output","2da imagen");
    _t("3rd output","3ra imagen");
    _t("4th output","4ta imagen");
    _t("1st -> 2nd","1ra -> 2da");
    _t("1st -> 3rd","1ra -> 3ra");
    _t("1st -> 4th","1ra -> 4ta");
    _t("All outputs","Todas las imagenes (salida)");
    _t("Output messages...","Mensajes de salida...");
    _t("Quiet (default)","Sin mensajes (predet.)");
    _t("Verbose (layer name)","Detallado (nombre de la capa)");
    _t("Verbose (console)","Detallado (consola)");
    _t("Verbose (logfile)","Detallado (archivo_registro)");
    _t("Very verbose (console)","Muy detallado (consola)");
    _t("Very verbose (logfile)","Muy detallado (archivo_registro)");
    _t("Debug mode (console)","Depuraci\303\263n (consola)");
    _t("Debug mode (logfile)","Depuraci\303\263n (archivo_registro)");
    _t("Preview size...","Tamano de previsualizaci\303\263n...");
    _t("Tiny","Muy pequena");
    _t("Small","Pequena");
    _t("Normal","Normal");
    _t("Large","Grande");
    _t(" Available filters (%u)"," Filtros disponibles (%u)");
    _t("Update","Actualitzaci\303\263n");
    _t("Rename","Renombrar");
    _t("Add filter to faves","A\303\261ade filtro a favoritos");
    _t("Remove filter from faves","Remueve filtro de favoritos");
    _t("Update filters","Actualiza filtros");
    _t("Enable Internet updates","Permite actualizaciones de Internet");
    _t("Expand/collapse","Expande/Colapsa");
    _t("Reset zoom","Restablecer zoom");
  }

  // English translation (default).
  if (!s) {
    const char *const ns = "Filters update from Internet (partially) failed !\n\n"
      "Unable to reach these filters sources:\n";
    return ns;
  }
  return s;
}

// Functions to sort filter tree view.
//------------------------------------
gint tree_view_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer) {
  char *name1, *name2;
  gtk_tree_model_get(model,a,2,&name1,-1);
  gtk_tree_model_get(model,b,2,&name2,-1);
  return cimg::strcasecmp(name1,name2);
}

const char *tree_view_sort_str(const char *str) {
  if (!str || !*str) return str;
  while (str[0]=='<' && str[1] && str[2]=='>') str+=3;
  return str;
}

// Flush filter tree view
//------------------------
void flush_tree_view(GtkWidget *const tree_view) {
  const unsigned int filter = get_current_filter();
  bool tree_mode = get_tree_mode();
  unsigned int current_dir = 0;
  CImg<char> current_path(64); *current_path = 0;
  gimp_get_data("gmic_current_treepath",current_path);
  if (tree_mode) { // Expand
    cimglist_for(gmic_1stlevel_entries,l) {
      GtkTreePath *path = gtk_tree_path_new_from_string(gmic_1stlevel_entries[l].data());
      gtk_tree_view_expand_row(GTK_TREE_VIEW(tree_view),path,false);
      gtk_tree_path_free(path);
    }
  } else { // Collapse
    if (filter && *current_path && cimg_sscanf(current_path,"%u",&current_dir)==1) {
      cimglist_for(gmic_1stlevel_entries,l) {
        const char *const s_path = gmic_1stlevel_entries[l].data();
        unsigned int dir = 0;
        if (cimg_sscanf(s_path,"%u",&dir)!=1 || dir!=current_dir) {
          GtkTreePath *path = gtk_tree_path_new_from_string(gmic_1stlevel_entries[l].data());
          gtk_tree_view_collapse_row(GTK_TREE_VIEW(tree_view),path);
          gtk_tree_path_free(path);
        }
      }
    } else gtk_tree_view_collapse_all(GTK_TREE_VIEW(tree_view));
  }
  if (filter && *current_path) {
    GtkTreePath *path = gtk_tree_path_new_from_string(current_path);
    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view),path);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view),path,NULL,FALSE,0,0);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    gtk_tree_selection_select_path(selection,path);
    gtk_tree_path_free(path);
  }
  if (indice_faves<gmic_entries.size()) { // Always shows 'Faves' folder when available.
    GtkTreePath *path = gtk_tree_path_new_from_string(gmic_1stlevel_entries[0].data());
    gtk_tree_view_expand_row(GTK_TREE_VIEW(tree_view),path,false);
    gtk_tree_path_free(path);
  }
  if (tree_mode_stock) gtk_widget_destroy(tree_mode_stock);
  tree_mode_stock = gtk_button_new_from_stock(tree_mode?GTK_STOCK_ZOOM_OUT:GTK_STOCK_ZOOM_IN);
  GtkWidget *tree_image = gtk_button_get_image(GTK_BUTTON(tree_mode_stock));
  gtk_button_set_image(GTK_BUTTON(tree_mode_button),tree_image);
  gtk_widget_show(tree_mode_button);

  gtk_tree_view_remove_column(GTK_TREE_VIEW(tree_view),gtk_tree_view_get_column(GTK_TREE_VIEW(tree_view),0));
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  CImg<char> tree_view_title(64);
  cimg_snprintf(tree_view_title,tree_view_title.width(),t(" Available filters (%u)"),nb_available_filters);
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(tree_view_title,renderer,"markup",1,NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),column);
}

// Retrieve files and update filter tree structure.
//-------------------------------------------------
CImgList<char> update_filters(const bool try_net_update, const bool is_silent=false) {

  // Build list of filter sources.
  CImgList<gmic_pixel_type> _sources;
  CImgList<char> _names;
  CImg<char> command(1024);
  cimg_snprintf(command,command.width(),
                "%s-gimp_filter_sources %d",
                get_verbosity_mode()>5?"-debug ":get_verbosity_mode()>3?"":"-v -99 ",
                try_net_update?1:0);
  try { gmic(command,_sources,_names,gmic_additional_commands,true); } catch (...) { }
  CImgList<char> sources;
  _sources.move_to(sources);
  cimglist_for(sources,l) {
    char &c = sources[l].unroll('x').back();
    if (c) {
      if (c==1) { c = 0; sources[l].columns(0,sources[l].width()); sources[l].back() = 1; }
      else sources[l].columns(0,sources[l].width());
    }
  }

  // Free existing definitions.
  const unsigned int old_nb_filters = get_nbfilters();
  if (tree_view_store) { g_object_unref(tree_view_store); tree_view_store = 0; }
  gmic_additional_commands.assign();
  gmic_1stlevel_entries.assign();
  gmic_faves.assign();
  gmic_entries.assign(1);
  gmic_commands.assign(1);
  gmic_preview_commands.assign(1);
  gmic_preview_factors.assign(1);
  gmic_arguments.assign(1);
  if (try_net_update && !is_silent) gimp_progress_init(" G'MIC: Update filters...");

  // Get filter definition files from external web servers.
  CImg<char> filename(1024);
  if (try_net_update && !is_silent) gimp_progress_pulse();
  CImg<char> filename_tmp(1024);
  CImgList<char> invalid_servers;
  char sep = 0;
  cimglist_for(sources,l) if (try_net_update && (!cimg::strncasecmp(sources[l],"http://",7) ||
                                                 !cimg::strncasecmp(sources[l],"https://",8))) {

    const char *const s_basename = gmic::basename(sources[l]);
    CImg<char> _s_basename = CImg<char>::string(s_basename);
    cimg::strwindows_reserved(_s_basename);
    if (!is_silent) gimp_progress_set_text_printf(" G'MIC: Update filters '%s'...",s_basename);
    cimg_snprintf(filename,filename.width(),"%s%s",
                  gmic::path_rc(),_s_basename.data());

    // Download filter file.
    if (get_verbosity_mode()>1) { // Verbose mode.
      std::fprintf(cimg::output(),"\n[gmic_gimp]./update/ Download new filter data from '%s'.\n",
                   sources[l].data());
      std::fflush(cimg::output());
    }

    const unsigned int omode = cimg::exception_mode();
    cimg::exception_mode() = 0;
    try {
      cimg::load_network(sources[l],filename_tmp,is_silent?4:60);
      std::FILE *file = std::fopen(filename_tmp,"rb");

      // Eventually, uncompress .cimgz file.
      if (file && (std::fscanf(file," #@gmi%c",&sep)!=1 || sep!='c')) {
        std::rewind(file);
        try {
          CImg<unsigned char> buffer; buffer.load_cimg(file); std::fclose(file);
          if (get_verbosity_mode()>1)
            std::fprintf(cimg::output(),
                         "\n[gmic_gimp]./update/ Uncompress file '%s' (was '%s').\n",
                         filename_tmp.data(),sources[l].data());
          buffer.save_raw(filename_tmp); file = std::fopen(filename_tmp,"rb"); }
        catch (...) { }
      }
      if (file) std::rewind(file);

      // Copy file to its final location.
      if (file && std::fscanf(file," #@gmi%c",&sep)==1 && sep=='c') {
        std::fclose(file);
        if (get_verbosity_mode()>1)
          std::fprintf(cimg::output(),
                       "\n[gmic_gimp]./update/ Copy temporary file '%s' at destination '%s'.\n",
                       filename_tmp.data(),filename.data());
        CImg<unsigned char>::get_load_raw(filename_tmp).save_raw(filename);
      } else invalid_servers.insert(sources[l]); // Failed in recognizing file header.

    } catch (...) { // Failed in downloading file.
      invalid_servers.insert(sources[l]);
    }
    cimg::exception_mode() = omode;
    if (!is_silent) gimp_progress_pulse();
    std::remove(filename_tmp);
  }
  if (!is_silent) gimp_progress_set_text(" G'MIC: Update filters...");

  // Read local source files.
  CImgList<char> _gmic_additional_commands;
  bool is_default_update = false;
  cimglist_for(sources,l) {
    const char *s_basename = gmic::basename(sources[l]);
    CImg<char> _s_basename = CImg<char>::string(s_basename);
    cimg::strwindows_reserved(_s_basename);
    if (!cimg::strncasecmp(sources[l],"http://",7) ||
        !cimg::strncasecmp(sources[l],"https://",8)) // Network file should have been copied in resources folder.
      cimg_snprintf(filename,filename.width(),"%s%s",
                    gmic::path_rc(),_s_basename.data());
    else // Local file, try to locate it at its hard-coded path.
      cimg_snprintf(filename,filename.width(),"%s",
                    sources[l].data());
    const unsigned int omode = cimg::exception_mode();
    try {
      CImg<char> com;
      bool add_code_separator = false;
      cimg::exception_mode(0);
      if (sources[l].back()==1) { // Overload default, add more checking.
        com.load_raw(filename);
        const char *_com = com.data(), *const ecom = com.end();
        while (_com<ecom && *_com<=32) ++_com;
        if (_com + 6<ecom && !std::strncmp(_com,"#@gmic",6)) {
          is_default_update = true;
          com.move_to(_gmic_additional_commands);
          add_code_separator = true;
        } else if (com.data() + 15<ecom && !std::strncmp(com,"1 unsigned_char",15)) {
          CImgList<char>::get_unserialize(com)[0].move_to(_gmic_additional_commands);
          is_default_update = true;
          add_code_separator = true;
        }
      } else {
        com.load_raw(filename);
        const char *const ecom = com.end();
        if (com.data() + 15<ecom && !std::strncmp(com,"1 unsigned_char",15))
          CImgList<char>::get_unserialize(com)[0].move_to(_gmic_additional_commands);
        else com.move_to(_gmic_additional_commands);
        add_code_separator = true;
      }
      if (add_code_separator)
        CImg<char>::string("\n#@gimp ________\n",false).unroll('y').move_to(_gmic_additional_commands);
    } catch(...) {
      if (get_verbosity_mode()>1)
        std::fprintf(cimg::output(),
                     "\n[gmic_gimp]./update/ Filter file '%s' not found.\n",
                     filename.data());
      std::fflush(cimg::output());
    }
    cimg::exception_mode(omode);
    if (try_net_update && !is_silent) gimp_progress_pulse();
  }

  if (!is_default_update) { // Add hardcoded default filters if no updates of the default commands.
    _gmic_additional_commands.insert(gmic::uncompress_stdlib());
    CImg<char>::string("\n#@gimp ________\n",false).unroll('y').move_to(_gmic_additional_commands);
  }

  cimglist_for(_gmic_additional_commands,l) { // Remove unusual characters.
    char *_p = _gmic_additional_commands[l];
    cimg_for(_gmic_additional_commands[l],p,char) if (*p!=13) *(_p++) = (unsigned char)*p<' ' && *p!=10?' ':*p;
    if (_p<_gmic_additional_commands[l].end())
      CImg<char>(_gmic_additional_commands[l].data(),1,_p - _gmic_additional_commands[l].data()).
        move_to(_gmic_additional_commands[l]);
  }

  CImg<char>::vector(0).move_to(_gmic_additional_commands);
  (_gmic_additional_commands>'y').move_to(gmic_additional_commands);

  // Add fave folder if necessary (make it before actually adding faves to make tree paths valids).
  CImgList<char> gmic_1stlevel_names;
  GtkTreeIter iter, fave_iter, parent[8];
  CImg<char> filename_gmic_faves(1024);
  tree_view_store = gtk_tree_store_new(3,G_TYPE_UINT,G_TYPE_STRING,G_TYPE_STRING);
  cimg_snprintf(filename_gmic_faves,filename_gmic_faves.width(),"%sgimp_faves",
                gmic::path_rc());
  std::FILE *file_gmic_faves = std::fopen(filename_gmic_faves,"rb");
  if (file_gmic_faves) {
    gtk_tree_store_append(tree_view_store,&fave_iter,0);
    gtk_tree_store_set(tree_view_store,&fave_iter,0,0,1,"<b>Faves</b>",2,"",-1);
    const char *treepath = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(tree_view_store),&fave_iter);
    CImg<char>::vector(0).move_to(gmic_1stlevel_names);
    CImg<char>::string(treepath).move_to(gmic_1stlevel_entries);
  }

  // Parse filters descriptions for GIMP, and create corresponding sorted treeview_store.
  CImg<char> line(256*1024), preview_command(256), arguments(65536), entry(256), locale = get_locale();
  *line = *preview_command = *arguments = *entry = 0;
  int level = 0, err = 0;
  bool is_testing = false;
  nb_available_filters = 0;
  cimg_snprintf(line,line.width(),"#@gimp_%s ",locale.data());

  // Use English for default language if no translated filters found.
  if (!std::strstr(gmic_additional_commands,line)) { locale[0] = 'e'; locale[1] = 'n'; locale[2] = 0; }
  for (const char *data = gmic_additional_commands; *data; ) {
    char *_line = line;
    // Read new line.
    while (*data!='\n' && *data && _line<line.data() + line.width()) *(_line++) = *(data++);
    *_line = 0;
    while (*data=='\n') ++data; // Skip next '\n'.
    for (_line = line; *_line; ++_line) if (*_line<' ') *_line = ' '; // Replace non-usual characters by spaces.
    if (line[0]!='#' || line[1]!='@' || line[2]!='g' || // Check for a '#@gimp' line.
        line[3]!='i' || line[4]!='m' || line[5]!='p') continue;
    if (line[6]=='_') { // Check for a localized filter.
      // Whether the entry match current locale or not.
      if (line[7]==locale[0] && line[8]==locale[1] && line[9]==' ') _line = line.data() + 10;
      else continue;
    } else if (line[6]==' ') _line = line.data() + 7; else continue; // Check for a non-localized filter.

    if (*_line!=':') { // Check for a description of a possible filter or menu folder.
      *entry = *command = *preview_command = *arguments = 0;
      err = cimg_sscanf(_line," %255[^:]: %1023[^,]%*c %255[^,]%*c %65533[^\n]",
                        entry.data(),command.data(),preview_command.data(),arguments.data());
      if (err==1) { // If entry defines a menu folder.
        cimg::strpare(entry,' ',false,true);
        char *nentry = entry; while (*nentry=='_') { ++nentry; --level; }
        if (level<0) level = 0; else if (level>7) level = 7;
        cimg::strpare(nentry,' ',false,true);
        cimg::strpare(nentry,'\"',true,false);
        if (*nentry) {
          if (level) {
            gtk_tree_store_append(tree_view_store,&parent[level],level?&parent[level - 1]:0);
            gtk_tree_store_set(tree_view_store,&parent[level],0,0,1,nentry,
                               2,tree_view_sort_str(nentry),-1);
          } else { // 1st-level folder.
            bool is_duplicate = false;
            cimglist_for(gmic_1stlevel_names,l)
              if (!std::strcmp(nentry,gmic_1stlevel_names[l].data())) { // Folder name is a duplicate.
                if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(tree_view_store),&parent[level],
                                                        gmic_1stlevel_entries[l].data())) {
                  is_duplicate = true;
                  break;
                }
              }

            // Detect if filter is in 'Testing/' (won't be counted in number of filters).
            gtk_label_set_markup(GTK_LABEL(markup2ascii),nentry);
            const char *_nentry = gtk_label_get_text(GTK_LABEL(markup2ascii));
            is_testing = !std::strcmp(_nentry,"Testing");

            if (!is_duplicate) {
              gtk_tree_store_append(tree_view_store,&parent[level],level?&parent[level - 1]:0);
              gtk_tree_store_set(tree_view_store,&parent[level],0,0,1,nentry,
                                 2,tree_view_sort_str(nentry),-1);
              const char *treepath = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(tree_view_store),
                                                                         &parent[level]);
              CImg<char>::string(nentry).move_to(gmic_1stlevel_names);
              CImg<char>::string(treepath).move_to(gmic_1stlevel_entries);
              unsigned int order = 0;
              for (unsigned int i = 0; i<4; ++i) {
                order<<=8;
                if (*_nentry) order|=(unsigned char)cimg::lowercase(*(_nentry++));
              }
            }
          }
          ++level;
        }
      } else if (err>=2) { // If entry defines a regular filter.
        cimg::strpare(entry,' ',false,true);
        char *nentry = entry; while (*nentry=='_') { ++nentry; --level; }
        if (level<0) level = 0; else if (level>7) level = 7;
        cimg::strpare(nentry,' ',false,true);
        cimg::strpare(nentry,'\"',true,false);
        cimg::strpare(command,' ',false,true);
        cimg::strpare(arguments,' ',false,true);
        if (*nentry) {
          CImg<char>::string(nentry).move_to(gmic_entries);
          CImg<char>::string(command).move_to(gmic_commands);
          CImg<char>::string(arguments).move_to(gmic_arguments);
          if (err>=3) { // Filter has a specified preview command.
            cimg::strpare(preview_command,' ',false,true);
            char *const preview_mode = std::strchr(preview_command,'(');
            bool is_accurate_when_zoomed = false;
            double factor = 1;
            char sep = 0;
            if (preview_mode &&
                ((cimg_sscanf(preview_mode + 1,"%lf)%c",&factor,&sep)==2 && sep=='+') ||
                 (cimg_sscanf(preview_mode + 1,"%lf%c",&factor,&sep)==2 && sep==')')) &&
                factor>=0) {
              *preview_mode = 0;
              is_accurate_when_zoomed = sep=='+';
            } else factor = -1;
            CImg<char>::string(preview_command).move_to(gmic_preview_commands);
            CImg<double>::vector(factor,(double)is_accurate_when_zoomed).move_to(gmic_preview_factors);
          } else {
            CImg<char>::string("_none_").move_to(gmic_preview_commands);
            CImg<double>::vector(-1,1).move_to(gmic_preview_factors);
          }
          gtk_tree_store_append(tree_view_store,&iter,level?&parent[level - 1]:0);
          gtk_tree_store_set(tree_view_store,&iter,0,gmic_entries.size() - 1,1,nentry,
                             2,tree_view_sort_str(nentry),-1);
          if (!level) {
            gtk_label_set_markup(GTK_LABEL(markup2ascii),nentry);
            const char *_nentry = gtk_label_get_text(GTK_LABEL(markup2ascii));
            unsigned int order = 0;
            for (unsigned int i = 0; i<3; ++i) { order<<=8; if (*_nentry) order|=cimg::lowercase(*(_nentry++)); }
          }
          if (!is_testing) ++nb_available_filters;  // Count only non-testing filters.
        }
      }
    } else { // Line is the continuation of an entry.
      if (gmic_arguments) {
        if (gmic_arguments.back()) gmic_arguments.back().back() = ' ';
        cimg::strpare(++_line,' ',false,true);
        gmic_arguments.back().append(CImg<char>(_line,std::strlen(_line) + 1,1,1,1,true),'x');
      }
    }
  }

  // Sort current GtkTreeModel in alphabetical order.
  GtkTreeSortable *const sortable = GTK_TREE_SORTABLE(tree_view_store);
  gtk_tree_sortable_set_sort_func(sortable,0,tree_view_sort_func,NULL,NULL);
  gtk_tree_sortable_set_sort_column_id(sortable,0,GTK_SORT_ASCENDING);

  if (try_net_update && !is_silent) gimp_progress_pulse();

  // Load faves.
  CImg<char> label(256);
  indice_faves = gmic_entries.size();
  if (file_gmic_faves) {
    for (unsigned int line_nb = 1; std::fscanf(file_gmic_faves," %[^\n]",line.data())==1; ++line_nb) {
      char sep = 0;
      if (cimg_sscanf(line,"{%255[^}]}{%255[^}]}{%255[^}]}{%255[^}]%c",
                      label.data(),entry.data(),command.data(),preview_command.data(),&sep)==5 && sep=='}') {
        const char *_line = line.data() + 8 + std::strlen(label) + std::strlen(entry) + std::strlen(command) +
          std::strlen(preview_command);
        int entry_found = -1, command_found = -1, preview_found = -1;
        unsigned int filter = 0;
        for (filter = 1; filter<indice_faves; ++filter) {
          const bool
            is_entry_match = !std::strcmp(gmic_entries[filter].data(),entry),
            is_command_match = !std::strcmp(gmic_commands[filter].data(),command),
            is_preview_match = !std::strcmp(gmic_preview_commands[filter].data(),preview_command);
          if (is_entry_match) entry_found = filter;
          if (is_command_match) command_found = filter;
          if (is_preview_match) preview_found = filter;
          if (is_command_match && is_preview_match) break;
        }

        CImg<char>::string(line).move_to(gmic_faves);
        // Get back '}' if necessary.
        for (char *p = std::strchr(label,gmic_rbrace); p; p = std::strchr(p,gmic_rbrace)) *p = '}';
        for (char *p = std::strchr(entry,gmic_rbrace); p; p = std::strchr(p,gmic_rbrace)) *p = '}';

        if (filter>=indice_faves) { // Entry not found.
          CImg<char>::string(label).move_to(gmic_entries);
          CImg<char>::string("_none_").move_to(gmic_commands);
          CImg<char>::string("_none_").move_to(gmic_preview_commands);
          cimg_sprintf(line,"note = note{\"<span foreground=\"red\"><b>Warning: </b></span>This fave links to an "
                       "unreferenced entry/set of G'MIC commands:\n\n"
                       "   - '<span foreground=\"purple\">%s</span>' as the entry name (%s%s%s%s%s).\n\n"
                       "   - '<span foreground=\"purple\">%s</span>' as the command to compute the filter "
                       "(%s%s%s%s%s).\n\n"
                       "   - '<span foreground=\"purple\">%s</span>' as the command to preview the filter "
                       "(%s%s%s%s%s)."
                       "\"}",
                       entry.data(),
                       entry_found>=0?"recognized, associated to <i>":"<b>not recognized</b>",
                       entry_found>=0?gmic_commands[entry_found].data():"",
                       entry_found>=0?", ":"",
                       entry_found>=0?gmic_preview_commands[entry_found].data():"",
                       entry_found>=0?"</i>":"",
                       command.data(),
                       command_found>=0?"recognized, associated to <i>":"<b>not recognized</b>",
                       command_found>=0?gmic_entries[command_found].data():"",
                       command_found>=0?", ":"",
                       command_found>=0?gmic_preview_commands[command_found].data():"",
                       command_found>=0?"</i>":"",
                       preview_command.data(),
                       preview_found>=0?"recognized, associated to <i>":"<b>not recognized</b>",
                       preview_found>=0?gmic_entries[preview_found].data():"",
                       preview_found>=0?", ":"",
                       preview_found>=0?gmic_commands[preview_found].data():"",
                       preview_found>=0?"</i>":"");

          CImg<char>::string(line).move_to(gmic_arguments);
          CImg<double>::vector(0,0).move_to(gmic_preview_factors);
          set_filter_nbparams(gmic_entries.size() - 1,0);
        } else { // Entry found.
          CImg<char>::string(label).move_to(gmic_entries);
          gmic_commands.insert(gmic_commands[filter]);
          gmic_preview_commands.insert(gmic_preview_commands[filter]);
          gmic_arguments.insert(gmic_arguments[filter]);
          gmic_preview_factors.insert(gmic_preview_factors[filter]);
          unsigned int nbp = 0;
          for (nbp = 0; cimg_sscanf(_line,"{%65533[^}]%c",arguments.data(),&sep)==2 && sep=='}'; ++nbp) {
            // Get back '}' if necessary.
            for (char *p = std::strchr(arguments,gmic_rbrace); p; p = std::strchr(p,gmic_rbrace)) *p = '}';
            // Get back '\n' if necessary.
            for (char *p = std::strchr(arguments,gmic_newline); p; p = std::strchr(p,gmic_newline)) *p = '\n';
            set_fave_parameter(gmic_entries.size() - 1,nbp,arguments);
            _line+=2 + std::strlen(arguments);
          }
          set_filter_nbparams(gmic_entries.size() - 1,nbp);
        }
        gtk_tree_store_append(tree_view_store,&iter,&fave_iter);
        gtk_tree_store_set(tree_view_store,&iter,0,gmic_entries.size() - 1,1,label.data(),
                           2,tree_view_sort_str(label),-1);
      } else if (get_verbosity_mode()>1)
        std::fprintf(cimg::output(),
                     "\n[gmic_gimp]./update/ Malformed line %u in fave file '%s': '%s'.\n",
                     line_nb,filename_gmic_faves.data(),line.data());
    }
    std::fclose(file_gmic_faves);
  }

  set_nbfilters(gmic_entries.size());
  if (try_net_update && !is_silent) {
    gimp_progress_update(1);
    gimp_progress_end();
  }

  if (gmic_entries.size()!=old_nb_filters) { reset_filters_parameters(); set_current_filter(0); }
  return invalid_servers;
}

// 'Convert' a CImg<T> image to a RGB[A] CImg<unsigned char> image, withing the same buffer.
//------------------------------------------------------------------------------------------
template<typename T>
void convert_image2uchar(CImg<T>& img) {
  const unsigned int siz = img.width()*img.height();
  unsigned char *ptrd = (unsigned char*)img.data();
  switch (img.spectrum()) {
  case 1 : {
    const T *ptr0 = img.data(0,0,0,0);
    for (unsigned int i = 0; i<siz; ++i) *(ptrd++) = (unsigned char)*(ptr0++);
  } break;
  case 2 : {
    const T *ptr0 = img.data(0,0,0,0), *ptr1 = img.data(0,0,0,1);
    for (unsigned int i = 0; i<siz; ++i) {
      ptrd[0] = (unsigned char)*(ptr0++);
      ptrd[1] = (unsigned char)*(ptr1++);
      ptrd+=2;
    }
  } break;
  case 3 : {
    const T *ptr0 = img.data(0,0,0,0), *ptr1 = img.data(0,0,0,1), *ptr2 = img.data(0,0,0,2);
    for (unsigned int i = 0; i<siz; ++i) {
      ptrd[0] = (unsigned char)*(ptr0++);
      ptrd[1] = (unsigned char)*(ptr1++);
      ptrd[2] = (unsigned char)*(ptr2++);
      ptrd+=3;
    }
  } break;
  case 4 : {
    const T *ptr0 = img.data(0,0,0,0), *ptr1 = img.data(0,0,0,1),
      *ptr2 = img.data(0,0,0,2), *ptr3 = img.data(0,0,0,3);
    for (unsigned int i = 0; i<siz; ++i) {
      ptrd[0] = (unsigned char)*(ptr0++);
      ptrd[1] = (unsigned char)*(ptr1++);
      ptrd[2] = (unsigned char)*(ptr2++);
      ptrd[3] = (unsigned char)*(ptr3++);
      ptrd+=4;
    }
  } break;
  default: return;
  }
}

// Apply current GIMP color profile to input image.
//-------------------------------------------------
// (Much thanks to Andrea Ferrero for his contribution on this).
template<typename T>
CImg<T>& apply_icc(CImg<T>& img) {
#if GIMP_MINOR_VERSION<=8
  cimg::unused(img);
#else
  if (!img || img.spectrum()<3 || img.spectrum()>4 || !img_profile) return img;

  GimpColorConfig *const color_config = gimp_get_color_configuration();
  if (!color_config) return img;
  const Babl *const fmt = babl_format(img.spectrum()==3?"R'G'B' float":"R'G'B'A float");
  GimpColorTransform *const transform = gimp_widget_get_color_transform(gui_preview,color_config,img_profile,fmt,fmt);
  if (!transform) return img;

  CImg<float> corrected;
  img.get_permute_axes("cxyz").move_to(corrected)/=255;
  gimp_color_transform_process_pixels(transform,fmt,corrected,fmt,corrected,corrected.height()*corrected.depth());
  (corrected.permute_axes("yzcx")*=255).cut(0,255).move_to(img);
  g_object_unref(transform);
#endif
  return img;
}

// Color-correct preview image.
//-----------------------------
void preview_with_icc() {
#if GIMP_MINOR_VERSION>8
  img_profile = gimp_image_get_effective_color_profile(image_id);
  GimpColorConfig *const color_config = gimp_get_color_configuration();
  if (!img_profile || !color_config) return;
  int wp, hp, sp;
  guchar *const ptr0 = gimp_zoom_preview_get_source(GIMP_ZOOM_PREVIEW(gui_preview),&wp,&hp,&sp);
  CImg<unsigned char> img(ptr0,sp,wp,hp);
  apply_icc(img.permute_axes("yzcx")).permute_axes("cxyz");
  std::memcpy(ptr0,img,wp*hp*sp*sizeof(unsigned char));
  gimp_preview_draw_buffer(GIMP_PREVIEW(gui_preview),ptr0,wp*sp);
  g_free(ptr0);
#endif
}

// Calibrate any image to fit the required number of channels (GRAY,GRAYA, RGB or RGBA).
//---------------------------------------------------------------------------------------
template<typename T>
void calibrate_image(CImg<T>& img, const unsigned int spectrum, const bool is_preview) {
  if (!img || !spectrum) return;
  switch (spectrum) {

  case 1 : // To GRAY
    switch (img.spectrum()) {
    case 1 : // from GRAY
      break;
    case 2 : // from GRAYA
      if (is_preview) {
        T *ptr_r = img.data(0,0,0,0), *ptr_a = img.data(0,0,0,1);
        cimg_forXY(img,x,y) {
          const unsigned int a = (unsigned int)*(ptr_a++), i = 96 + (((x^y)&8)<<3);
          *ptr_r = (T)((a*(unsigned int)*ptr_r + (255 - a)*i)>>8);
          ++ptr_r;
        }
      }
      img.channel(0);
      break;
    case 3 : // from RGB
      (img.get_shared_channel(0)+=img.get_shared_channel(1)+=img.get_shared_channel(2))/=3;
      img.channel(0);
      break;
    case 4 : // from RGBA
      (img.get_shared_channel(0)+=img.get_shared_channel(1)+=img.get_shared_channel(2))/=3;
      if (is_preview) {
        T *ptr_r = img.data(0,0,0,0), *ptr_a = img.data(0,0,0,3);
        cimg_forXY(img,x,y) {
          const unsigned int a = (unsigned int)*(ptr_a++), i = 96 + (((x^y)&8)<<3);
          *ptr_r = (T)((a*(unsigned int)*ptr_r + (255 - a)*i)>>8);
          ++ptr_r;
        }
      }
      img.channel(0);
      break;
    default : // from multi-channel (>4)
      img.channel(0);
    } break;

  case 2: // To GRAYA
    switch (img.spectrum()) {
    case 1: // from GRAY
      img.resize(-100,-100,1,2,0).get_shared_channel(1).fill(255);
      break;
    case 2: // from GRAYA
      break;
    case 3: // from RGB
      (img.get_shared_channel(0)+=img.get_shared_channel(1)+=img.get_shared_channel(2))/=3;
      img.channels(0,1).get_shared_channel(1).fill(255);
      break;
    case 4: // from RGBA
      (img.get_shared_channel(0)+=img.get_shared_channel(1)+=img.get_shared_channel(2))/=3;
      img.get_shared_channel(1) = img.get_shared_channel(3);
      img.channels(0,1);
      break;
    default: // from multi-channel (>4)
      img.channels(0,1);
    } break;

  case 3: // to RGB
    switch (img.spectrum()) {
    case 1: // from GRAY
      img.resize(-100,-100,1,3);
      break;
    case 2: // from GRAYA
      if (is_preview) {
        T *ptr_r = img.data(0,0,0,0), *ptr_a = img.data(0,0,0,1);
        cimg_forXY(img,x,y) {
          const unsigned int a = (unsigned int)*(ptr_a++), i = 96 + (((x^y)&8)<<3);
          *ptr_r = (T)((a*(unsigned int)*ptr_r + (255 - a)*i)>>8);
          ++ptr_r;
        }
      }
      img.channel(0).resize(-100,-100,1,3);
      break;
    case 3: // from RGB
      break;
    case 4: // from RGBA
      if (is_preview) {
        T *ptr_r = img.data(0,0,0,0), *ptr_g = img.data(0,0,0,1),
          *ptr_b = img.data(0,0,0,2), *ptr_a = img.data(0,0,0,3);
        cimg_forXY(img,x,y) {
          const unsigned int a = (unsigned int)*(ptr_a++), i = 96 + (((x^y)&8)<<3);
          *ptr_r = (T)((a*(unsigned int)*ptr_r + (255 - a)*i)>>8);
          *ptr_g = (T)((a*(unsigned int)*ptr_g + (255 - a)*i)>>8);
          *ptr_b = (T)((a*(unsigned int)*ptr_b + (255 - a)*i)>>8);
          ++ptr_r; ++ptr_g; ++ptr_b;
        }
      }
      img.channels(0,2);
      break;
    default: // from multi-channel (>4)
      img.channels(0,2);
    } break;

  case 4: // to RGBA
    switch (img.spectrum()) {
    case 1: // from GRAY
      img.resize(-100,-100,1,4).get_shared_channel(3).fill(255);
      break;
    case 2: // from GRAYA
      img.resize(-100,-100,1,4,0);
      img.get_shared_channel(3) = img.get_shared_channel(1);
      img.get_shared_channel(1) = img.get_shared_channel(0);
      img.get_shared_channel(2) = img.get_shared_channel(0);
      break;
    case 3: // from RGB
      img.resize(-100,-100,1,4,0).get_shared_channel(3).fill(255);
      break;
    case 4: // from RGBA
      break;
    default: // from multi-channel (>4)
      img.channels(0,3);
    } break;
  }
}

// Get the input layers of a GIMP image as a list of CImg<T>.
//-----------------------------------------------------------
template<typename T>
CImg<int> get_input_layers(CImgList<T>& images, const unsigned int input_mode=~0U) {

  // Retrieve the list of desired layers.
  int
    nb_layers = 0,
    *layers = gimp_image_get_layers(image_id,&nb_layers),
    active_layer_id = gimp_image_get_active_layer(image_id);

  CImg<int> input_layers;
  if (gimp_item_is_group(active_layer_id)) { images.assign(); return input_layers; }

  const unsigned int _input_mode = input_mode!=~0U?input_mode:get_input_mode();
  switch (_input_mode) {
  case 0 : // Input none
    break;
  case 1 : // Input active layer
    if (active_layer_id>=0) input_layers = CImg<int>::vector(active_layer_id);
    break;
  case 2 : case 9 : // Input all image layers
    input_layers = CImg<int>(layers,1,nb_layers);
    if (_input_mode==9) input_layers.mirror('y');
    break;
  case 3 : // Input active & below layers
    if (active_layer_id>=0) {
      int i = 0; for (i = 0; i<nb_layers; ++i) if (layers[i]==active_layer_id) break;
      if (i<nb_layers - 1) input_layers = CImg<int>::vector(active_layer_id,layers[i + 1]);
      else input_layers = CImg<int>::vector(active_layer_id);
    } break;
  case 4 : // Input active & above layers
    if (active_layer_id>=0) {
      int i = 0; for (i = 0; i<nb_layers; ++i) if (layers[i]==active_layer_id) break;
      if (i>0) input_layers = CImg<int>::vector(active_layer_id,layers[i - 1]);
      else input_layers = CImg<int>::vector(active_layer_id);
    } break;
  case 5 : case 7 : { // Input all visible image layers
    CImgList<int> visible_layers;
    for (int i = 0; i<nb_layers; ++i)
      if (_gimp_item_get_visible(layers[i])) CImg<int>::vector(layers[i]).move_to(visible_layers);
    input_layers = visible_layers>'y';
    if (_input_mode==7) input_layers.mirror('y');
  } break;
  default : { // Input all invisible image layers
    CImgList<int> invisible_layers;
    for (int i = 0; i<nb_layers; ++i)
      if (!_gimp_item_get_visible(layers[i])) CImg<int>::vector(layers[i]).move_to(invisible_layers);
    input_layers = invisible_layers>'y';
    if (_input_mode==8) input_layers.mirror('y');
  } break;
  }

  // Read input image data into a CImgList<T>.
  images.assign(input_layers.height());
  gint rgn_x, rgn_y, rgn_width, rgn_height;
  cimglist_for(images,l) {
    if (!_gimp_item_is_valid(input_layers[l])) continue;
    if (!gimp_drawable_mask_intersect(input_layers[l],&rgn_x,&rgn_y,&rgn_width,&rgn_height)) continue;
    const int spectrum = (gimp_drawable_is_rgb(input_layers[l])?3:1) +
      (gimp_drawable_has_alpha(input_layers[l])?1:0);
#if GIMP_MINOR_VERSION<=8
    GimpDrawable *drawable = gimp_drawable_get(input_layers[l]);
    GimpPixelRgn region;
    gimp_pixel_rgn_init(&region,drawable,rgn_x,rgn_y,rgn_width,rgn_height,false,false);
    CImg<unsigned char> img(spectrum,rgn_width,rgn_height);
    gimp_pixel_rgn_get_rect(&region,img,rgn_x,rgn_y,rgn_width,rgn_height);
    gimp_drawable_detach(drawable);
    img.permute_axes("yzcx");
#else
    GeglRectangle rect;
    gegl_rectangle_set(&rect,rgn_x,rgn_y,rgn_width,rgn_height);
    GeglBuffer *buffer = gimp_drawable_get_buffer(input_layers[l]);
    const char *const format = spectrum==1?"Y' " s_gmic_pixel_type:spectrum==2?"Y'A " s_gmic_pixel_type:
      spectrum==3?"R'G'B' " s_gmic_pixel_type:"R'G'B'A " s_gmic_pixel_type;
    CImg<float> img(spectrum,rgn_width,rgn_height);
    gegl_buffer_get(buffer,&rect,1,babl_format(format),img.data(),0,GEGL_ABYSS_NONE);
    (img*=255).permute_axes("yzcx");
    g_object_unref(buffer);
#endif
    img.move_to(images[l]);
  }

  return input_layers;
}

// Return the G'MIC command line needed to run the selected filter.
//-----------------------------------------------------------------
CImg<char> get_command_line(const bool is_preview) {
  CImg<char> res;
  const unsigned int
    filter = get_current_filter(),
    nbparams = get_filter_nbparams(filter);
  if (!filter) return res;
  CImgList<char> lres;
  switch (get_verbosity_mode()) {
  case 0: case 1: case 2: case 3: CImg<char>("-v -99 -",8).move_to(lres); break;  // Quiet or Verbose.
  case 4: case 5 : CImg<char>("-v 0 -",6).move_to(lres); break;                   // Very verbose.
  default: CImg<char>("-debug -",8).move_to(lres);                                // Debug.
  }
  const CImg<char> &command_item = (is_preview?gmic_preview_commands[filter]:gmic_commands[filter]);
  if (command_item) {
    lres.insert(command_item);
    if (nbparams) {
      lres[1].back() = ' ';
      for (unsigned int p = 0; p<nbparams; ++p) {
        const CImg<char> _ss = get_filter_parameter(filter,p);
        const char *ss = _ss;
        const unsigned int l = (unsigned int)std::strlen(ss);
        CImg<char> nparam(l + 1);
        *nparam = 0;
        char *sd = nparam.data();
        if (l>=2 && *ss=='\"' && ss[l - 1]=='\"') { // Replace special characters in a string or a filename.
          ++ss; *(sd++) = '\"';
          for (unsigned int i = 1; i<l - 1; ++i, ++ss) { const char c = *ss; *(sd++) = c=='\"'?gmic_dquote:c; }
          *(sd++) = '\"'; *(sd++) = 0;
          nparam.move_to(lres);
        } else CImg<char>(ss,l + 1).move_to(lres);
        lres.back().back() = ',';
      }
    }
    (res=lres>'x').back() = 0;
  }
  return res;
}

// Set defaut zoom factor for preview of the current filter.
//----------------------------------------------------------
void set_preview_factor() {
  const unsigned int filter = get_current_filter();
  if (filter && gmic_preview_factors[filter] && GIMP_IS_PREVIEW(gui_preview)) {
    double factor = gmic_preview_factors(filter,0);
    if (factor>=0) {
      if (!factor) { // Compute factor so that 1:1 preview of the image is displayed.
        const float
          pw = (float)gimp_preview_width,
          ph = (float)gimp_preview_height;
#if GIMP_MINOR_VERSION<=8
        const float
          dw = (float)gimp_zoom_preview_get_drawable(GIMP_ZOOM_PREVIEW(gui_preview))->width,
          dh = (float)gimp_zoom_preview_get_drawable(GIMP_ZOOM_PREVIEW(gui_preview))->height;
#else
        const int preview_drawable_id = gimp_zoom_preview_get_drawable_id(GIMP_ZOOM_PREVIEW(gui_preview));
        const float
          dw = (float)gimp_drawable_width(preview_drawable_id),
          dh = (float)gimp_drawable_height(preview_drawable_id);
#endif
        factor = std::sqrt((dw*dw + dh*dh)/(pw*pw + ph*ph));
      }
      gimp_zoom_model_zoom(gimp_zoom_preview_get_model(GIMP_ZOOM_PREVIEW(gui_preview)),GIMP_ZOOM_TO,factor);
    }
  }
}

// Process image data with the G'MIC interpreter.
//-----------------------------------------------

// This structure stores the arguments required by the processing thread.
struct st_process_thread {
  CImgList<gmic_pixel_type> images;
  CImgList<char> images_names;
  CImg<char> env, error_message, status;
  bool is_thread, is_preview;
  unsigned int verbosity_mode;
  const char *command_line;
  float progress;
  bool is_abort;
  pthread_mutex_t is_running, wait_lock;
  pthread_cond_t wait_cond;
  pthread_t thread;
  void set_env() { // Must be called from main thread to avoid crash when doing 'gimp_get_data()'.
    env.assign(256);
    cimg_snprintf(env,env.width(),
                  "-v - "
                  "_input_layers=%u "
                  "_output_mode=%u "
                  "_output_messages=%u "
                  "_preview_mode=%u "
                  "_preview_size=%u "
                  "_preview_width=%d "
                  "_preview_height=%d",
                  get_input_mode(),get_output_mode(),get_verbosity_mode(),get_preview_mode(),get_preview_size(),
                  gimp_preview_width,gimp_preview_height);
  }
};

// Thread that runs the G'MIC interpreter.
void *process_thread(void *arg) {
  st_process_thread &spt = *(st_process_thread*)arg;
  if (spt.is_thread) {
    pthread_mutex_lock(&spt.is_running);
    pthread_mutex_lock(&spt.wait_lock);
    pthread_cond_signal(&spt.wait_cond);
    pthread_mutex_unlock(&spt.wait_lock);
  }
  try {
    if (spt.verbosity_mode>1) {
      CImg<char> cl = CImg<char>::string(spt.command_line);
      std::fprintf(cimg::output(),
                   "\n[gmic_gimp]./%s/ %s\n",
                   spt.is_preview?"preview":"apply",
                   cl.data());
      std::fflush(cimg::output());
    }
    gmic gmic_instance(spt.env,gmic_additional_commands,true);
    gmic_instance.run(spt.command_line,spt.images,spt.images_names,&spt.progress,&spt.is_abort);
    gmic_instance.status.move_to(spt.status);
  } catch (gmic_exception &e) {
    spt.images.assign();
    spt.images_names.assign();
    CImg<char>::string(e.what()).move_to(spt.error_message);
    if (spt.verbosity_mode>1) {
      std::fprintf(cimg::output(),
                   "\n[gmic_gimp]./error/ %s\n",
                   spt.error_message.data());
      std::fflush(cimg::output());
    }
  }
  if (spt.is_thread) {
    pthread_mutex_unlock(&spt.is_running);
    pthread_exit(0);
  }
  return 0;
}

// Handle GUI event functions.
//----------------------------
void create_parameters_gui(const bool);
void process_image(const char *const, const bool is_apply);
void process_preview();

void on_preview_button_changed(GtkToggleButton *const toggle_button) {
  cimg::mutex(25);
  if (p_spt) { st_process_thread &spt = *(st_process_thread*)p_spt; spt.is_abort = true; }
  cimg::mutex(25,0);
  if (!gtk_toggle_button_get_active(toggle_button)) {
    gtk_widget_hide(gui_preview_warning);
    preview_with_icc();
  }
}

void on_dialog_reset_zoom_button_clicked(GtkCheckButton *const) {
  set_preview_factor();
}

// Secure function for invalidating preview.
void _gimp_preview_invalidate() {
  cimg::mutex(25);
  if (p_spt) { st_process_thread &spt = *(st_process_thread*)p_spt; spt.is_abort = true; }
  cimg::mutex(25,0);
  const int active_layer_id = gimp_image_get_active_layer(image_id);
  if (active_layer_id>=0 && gimp_layer_get_edit_mask(active_layer_id))
    gimp_layer_set_edit_mask(active_layer_id,(gboolean)0);

  computed_preview.assign();

#if GIMP_MINOR_VERSION<=8
  const bool is_valid_preview_drawable = gui_preview && GIMP_IS_PREVIEW(gui_preview) &&
    _gimp_item_is_valid(gimp_zoom_preview_get_drawable(GIMP_ZOOM_PREVIEW(gui_preview))->drawable_id);
#else
  const bool is_valid_preview_drawable = gui_preview && GIMP_IS_PREVIEW(gui_preview) &&
    gimp_item_is_valid(gimp_zoom_preview_get_drawable_id(GIMP_ZOOM_PREVIEW(gui_preview)));
#endif

  if (is_valid_preview_drawable) gimp_preview_invalidate(GIMP_PREVIEW(gui_preview));
  else {
    if (GTK_IS_WIDGET(gui_preview)) gtk_widget_destroy(gui_preview);

    int is_selection = 0, sel_x0 = 0, sel_y0 = 0, sel_x1 = 0, sel_y1 = 0;
    if (!gimp_selection_bounds(image_id,&is_selection,&sel_x0,&sel_y0,&sel_x1,&sel_y1)) is_selection = 0;
    const int w = gimp_image_width(image_id), h = gimp_image_height(image_id);
    if (preview_image_id) gimp_image_delete(preview_image_id);
    preview_image_id = 0;
    preview_image_factor = 1;

    // Pre-compute image thumbnail for preview if image is too small.
    const int min_preview_size = (200 + 120*get_preview_size(true))*2/3;
    if (!is_selection && std::max(w,h)<min_preview_size) {
      int pw = 0, ph = 0;
      if (w>=h) ph = std::max(1,h*(pw=min_preview_size)/w);
      else pw = std::max(1,w*(ph=min_preview_size)/h);
      preview_image_id = gimp_image_duplicate(image_id);
      preview_image_factor = (double)std::max(pw,ph)/std::max(w,h);
      const GimpInterpolationType mode = gimp_context_get_interpolation();
      gimp_context_set_interpolation(GIMP_INTERPOLATION_NONE);
      gimp_image_scale(preview_image_id,pw,ph);
      gimp_context_set_interpolation(mode);
    }

#if GIMP_MINOR_VERSION<=8
    GimpDrawable *const preview_drawable =
      gimp_drawable_get(gimp_image_get_active_drawable(preview_image_id?preview_image_id:image_id));
    gui_preview = gimp_zoom_preview_new(preview_drawable);
#else
    const int preview_drawable_id = gimp_image_get_active_drawable(preview_image_id?preview_image_id:image_id);
    gui_preview = gimp_zoom_preview_new_from_drawable_id(preview_drawable_id);
#endif

    GtkWidget *const controls = gimp_preview_get_controls(GIMP_PREVIEW(gui_preview));
    GList *const children1 = ((GtkBox*)controls)->children;
    GtkBoxChild *const child1 = (GtkBoxChild*)children1->data;
    GtkWidget *const preview_button = child1->widget;
    g_signal_connect(preview_button,"toggled",G_CALLBACK(on_preview_button_changed),0);

    // Add 'reset zoom' button.
    GList *const children2 = children1->next;
    GtkBoxChild *const child2 = (GtkBoxChild*)children2->data;
    GtkWidget
      *const zoom_buttons = child2->widget,
      *const reset_zoom_button = gtk_button_new();
    reset_zoom_stock = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
    GtkWidget *const reset_zoom_image = gtk_button_get_image(GTK_BUTTON(reset_zoom_stock));
    gtk_button_set_image(GTK_BUTTON(reset_zoom_button),reset_zoom_image);
    gtk_widget_show(reset_zoom_button);
    gtk_box_pack_start(GTK_BOX(zoom_buttons),reset_zoom_button,false,false,0);
    gtk_widget_set_tooltip_text(reset_zoom_button,t("Reset zoom"));
    g_signal_connect(reset_zoom_button,"clicked",G_CALLBACK(on_dialog_reset_zoom_button_clicked),0);
    gtk_widget_show(gui_preview);
    gtk_container_add(GTK_CONTAINER(gui_preview_align),gui_preview);
    GtkRequisition requisition;
    gtk_widget_size_request(gui_preview_align,&requisition);
    gtk_widget_set_size_request(gui_preview_align,
                                -1,requisition.height<min_preview_size?min_preview_size:-1);
    g_signal_connect(gui_preview,"invalidated",G_CALLBACK(process_preview),0);
    gimp_preview_get_size(GIMP_PREVIEW(gui_preview),&gimp_preview_width,&gimp_preview_height);
  }
}

// Resize preview widget.
void resize_preview(const unsigned int size) {
  CImg<char> tmp(256);
  cimg_sprintf(tmp,
               "style \"gimp-large-preview\"\n"
               "{\n"
               "  GimpPreview::size = %u\n"
               "}\n"
               "class \"GimpPreview\" style \"gimp-large-preview\"",
               200 + size*120);
  gtk_rc_parse_string(tmp);
  if (GIMP_IS_PREVIEW(gui_preview)) {
    gtk_widget_destroy(gui_preview);
    gui_preview = 0;
    _gimp_preview_invalidate();
  }
}

// Reset value of all button parameters for current filter.
void reset_button_parameters() {
  const unsigned int filter = get_current_filter();
  cimglist_for(gmic_button_parameters,l) set_filter_parameter(filter,gmic_button_parameters(l,0),"0");
}

// Handle preview resize event.
void on_dialog_resized() {
  reset_button_parameters();
  static int old_pw = 0, old_ph = 0;
  if (GIMP_IS_PREVIEW(gui_preview)) {
    if (!old_pw || !old_ph) computed_preview.assign();
    else if (gimp_preview_width!=old_pw || gimp_preview_height!=old_ph) {
      set_preview_factor();
      computed_preview.assign();
    }
  }
  old_pw = gimp_preview_width;
  old_ph = gimp_preview_height;
  computed_preview.assign();
}

// Handle widgets events related to parameter changes.
void on_float_parameter_changed(GtkAdjustment *const adjustment, const void *const event_infos) {
  reset_button_parameters();
  double value = 0;
  gimp_double_adjustment_update(adjustment,&value);
  CImg<char> s_value(32);
  cimg_snprintf(s_value,s_value.width(),"%g",value);
  set_filter_parameter(get_current_filter(),*(int*)event_infos,s_value);
  _create_dialog_gui = true;
}

void on_int_parameter_changed(GtkAdjustment *const adjustment, const void *const event_infos) {
  reset_button_parameters();
  int value = 0;
  gimp_int_adjustment_update(adjustment,&value);
  CImg<char> s_value(32);
  cimg_snprintf(s_value,s_value.width(),"%d",value);
  set_filter_parameter(get_current_filter(),*(int*)event_infos,s_value);
  _create_dialog_gui = true;
}

void on_bool_parameter_changed(GtkCheckButton *const checkbutton, const void *const event_infos) {
  reset_button_parameters();
  int value = 0;
  g_object_get(checkbutton,"active",&value,NULL);
  CImg<char> s_value(4);
  cimg_snprintf(s_value,s_value.width(),"%d",value?1:0);
  set_filter_parameter(get_current_filter(),*(int*)event_infos,s_value);
  _create_dialog_gui = true;
}

void on_button_parameter_clicked(GtkCheckButton *const button, const void *const event_infos) {
  reset_button_parameters();
  cimg::unused(button);
  const unsigned int filter = get_current_filter();
  set_filter_parameter(filter,*(int*)event_infos,"1");
  _create_dialog_gui = true;
}

void on_list_parameter_changed(GtkComboBox *const combobox, const void *const event_infos) {
  reset_button_parameters();
  int value = 0;
  g_object_get(combobox,"active",&value,NULL);
  CImg<char> s_value(32);
  cimg_snprintf(s_value,s_value.width(),"%d",value);
  set_filter_parameter(get_current_filter(),*(int*)event_infos,s_value);
  _create_dialog_gui = true;
}

void on_text_parameter_changed(const void *const event_infos) {
  reset_button_parameters();
  GtkWidget *entry = *((GtkWidget**)event_infos + 1);
  const char *const s_value = gtk_entry_get_text(GTK_ENTRY(entry));
  CImg<char> s_param;
  if (s_value && *s_value) {
    CImg<char> _s_value = CImg<char>::string(s_value);
    cimg_for(_s_value,ptr,char) if (*ptr=='\"') *ptr = gmic_dquote;
    s_param.assign(_s_value.width() + 2);
    cimg_snprintf(s_param,s_param.width(),"\"%s\"",_s_value.data());
  } else std::strcpy(s_param.assign(3),"\"\"");
  set_filter_parameter(get_current_filter(),*(int*)event_infos,s_param);
  _create_dialog_gui = true;
}

void on_multitext_parameter_changed(const void *const event_infos) {
  reset_button_parameters();
  GtkWidget *text_view = *((GtkWidget**)event_infos + 1);
  GtkTextBuffer *const text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
  GtkTextIter it_start, it_end;
  gtk_text_buffer_get_bounds(text_buffer,&it_start,&it_end);
  const char *const s_value = gtk_text_buffer_get_text(text_buffer,&it_start,&it_end,false);
  CImg<char> s_param;
  if (s_value && *s_value) {
    CImg<char> _s_value = CImg<char>::string(s_value);
    cimg_for(_s_value,ptr,char) if (*ptr=='\"') *ptr = gmic_dquote;
    s_param.assign(_s_value.width() + 2);
    cimg_snprintf(s_param,s_param.width(),"\"%s\"",_s_value.data());
  } else std::strcpy(s_param.assign(3),"\"\"");
  set_filter_parameter(get_current_filter(),*(int*)event_infos,s_param);
  _create_dialog_gui = true;
}

void on_file_parameter_changed(GtkFileChooser *const file_chooser, const void *const event_infos) {
  reset_button_parameters();
  const char
    *const _s_value = gtk_file_chooser_get_filename(file_chooser),
    *const s_value = _s_value?_s_value:"";
  CImg<char> o_value = get_filter_parameter(get_current_filter(),*(int*)event_infos);
  cimg::strpare(o_value,'\"',true,false);
  const bool
    is_same_file = !std::strcmp(gmic::basename(s_value),gmic::basename(o_value)),
    is_silent_argument = (bool)*((void**)event_infos + 1);
  CImg<char> s_param;
  if (s_value && *s_value) {
    s_param.assign(std::strlen(s_value) + 3);
    cimg_snprintf(s_param,s_param.width(),"\"%s\"",s_value);
  } else std::strcpy(s_param.assign(3),"\"\"");
  set_filter_parameter(get_current_filter(),*(int*)event_infos,s_param);
  if (!is_same_file && !is_silent_argument)
    _gimp_preview_invalidate(); // Invalidate preview only if filename has changed.
  _create_dialog_gui = true;
}

void on_color_parameter_changed(GtkColorButton *const color_button, const void *const event_infos) {
  reset_button_parameters();
  GdkColor color;
  gtk_color_button_get_color(color_button,&color);
  CImg<char> s_value(256);
  if (gtk_color_button_get_use_alpha(color_button))
    cimg_snprintf(s_value,s_value.width(),"%d,%d,%d,%d",
                  color.red/257,color.green/257,color.blue/257,gtk_color_button_get_alpha(color_button)/257);
  else
    cimg_snprintf(s_value,s_value.width(),"%d,%d,%d",
                  color.red/257,color.green/257,color.blue/257);
  set_filter_parameter(get_current_filter(),*(int*)event_infos,s_value);
  _create_dialog_gui = true;
}

// Handle responses to the dialog window buttons.
void on_dialog_input_mode_changed(GtkComboBox *const combobox) {
  reset_button_parameters();
  int value = 0;
  g_object_get(combobox,"active",&value,NULL);
  if (value<2) gtk_combo_box_set_active(combobox,value=3);
  set_input_mode((unsigned int)value);
  _gimp_preview_invalidate();
}

void on_dialog_output_mode_changed(GtkComboBox *const combobox) {
  reset_button_parameters();
  int value = 0;
  g_object_get(combobox,"active",&value,NULL);
  if (value<2) gtk_combo_box_set_active(combobox,value=2);
  set_output_mode((unsigned int)value);
}

void on_dialog_verbosity_mode_changed(GtkComboBox *const combobox) {
  reset_button_parameters();
  int value = 0;
  g_object_get(combobox,"active",&value,NULL);
  if (value<2) gtk_combo_box_set_active(combobox,value=2);
  set_verbosity_mode((unsigned int)value);
  set_logfile();
  if (value>3) _gimp_preview_invalidate();
}

void on_dialog_preview_mode_changed(GtkComboBox *const combobox) {
  reset_button_parameters();
  int value = 0;
  g_object_get(combobox,"active",&value,NULL);
  if (value<2) gtk_combo_box_set_active(combobox,value=2);
  set_preview_mode((unsigned int)value);
  _gimp_preview_invalidate();
}

void on_dialog_preview_size_changed(GtkComboBox *const combobox) {
  reset_button_parameters();
  int value = 0;
  g_object_get(combobox,"active",&value,NULL);
  if (value<2) gtk_combo_box_set_active(combobox,value=2);
  set_preview_size((unsigned int)value);
  resize_preview(value - 2);
}

void on_dialog_maximize_button_clicked(GtkButton *const button) {
  reset_button_parameters();
  static int ow = 0, oh = 0;
  GdkScreen* screen = gtk_window_get_screen(GTK_WINDOW(dialog_window));
  int
    width = gdk_screen_get_width(screen),
    height = gdk_screen_get_height(screen);
  if (width>0 && height>0 && !ow && !oh) {
    gtk_window_get_size(GTK_WINDOW(dialog_window),&ow,&oh);
#if cimg_OS==1
    height-=64;
#elif cimg_OS==2
    // Subtract the height of the taskbar on windows.
    RECT rect;
    HWND taskBar = FindWindow("Shell_traywnd",NULL);
    if (taskBar && GetWindowRect(taskBar,&rect)) height-=rect.bottom - rect.top;
#endif
    if (height>0) gtk_window_resize(GTK_WINDOW(dialog_window),width,height);
    gtk_window_move(GTK_WINDOW(dialog_window),0,0);
    gtk_button_set_label(button,GTK_STOCK_LEAVE_FULLSCREEN);
  } else if (ow>0 && oh>0) {
    gtk_window_resize(GTK_WINDOW(dialog_window),ow,oh);
    ow = oh = 0;
    gtk_button_set_label(button,GTK_STOCK_FULLSCREEN);
  }
  _gimp_preview_invalidate();
}

void on_dialog_reset_clicked() {
  create_parameters_gui(true);
  _create_dialog_gui = true;
  _gimp_preview_invalidate();
}

void on_dialog_cancel_clicked() {
  cimg::mutex(25);
  if (p_spt) { st_process_thread &spt = *(st_process_thread*)p_spt; spt.is_abort = true; }
  cimg::mutex(25,0);
  reset_button_parameters();
  _create_dialog_gui = false;
  gtk_main_quit();
}

void on_dialog_apply_clicked() {
  gtk_widget_hide(dialog_window);
  while (gtk_events_pending()) gtk_main_iteration();
  process_image(0,true);
  gtk_widget_show(dialog_window);
  _create_dialog_gui = false;
  _gimp_preview_invalidate();
  const CImg<char> command_line = get_command_line(false);
  if (command_line) { // Remember command line for the next use of the filter.
    CImg<char> s_tmp(48);
    cimg_snprintf(s_tmp,s_tmp.width(),"gmic_command_line%u",get_current_filter());
    gimp_set_data(s_tmp,command_line,std::strlen(command_line) + 1);
  }
}

void on_dialog_net_update_toggled(GtkToggleButton *const toggle_button) {
  reset_button_parameters();
  set_net_update(gtk_toggle_button_get_active(toggle_button));
}

void on_dialog_tree_mode_clicked(GtkWidget *const tree_view) {
  reset_button_parameters();
  set_tree_mode(!get_tree_mode());
  flush_tree_view(tree_view);
}

void on_dialog_add_fave_clicked(GtkWidget *const tree_view) {
  reset_button_parameters();
  const unsigned int filter = get_current_filter();
  gtk_widget_hide(relabel_hbox);
  gtk_widget_hide(fave_delete_button);
  if (filter) {
    CImg<char> filename(1024);
    cimg_snprintf(filename,filename.width(),"%sgimp_faves",
                  gmic::path_rc());
    std::FILE *file = std::fopen(filename,"wb");
    if (file) {
      CImg<char> basename(256), label(256);
      *basename = *label = 0;
      unsigned int ind = 0;
      std::strcpy(basename,gmic_entries[filter].data());
      char *last_space = 0;
      for (char *p = basename; p; p = std::strchr(p + 1,' ')) last_space = p;
      if (last_space>basename) {
        char sep = 0, end = 0;
        if (cimg_sscanf(last_space + 1,"(%u%c%c",&ind,&sep,&end)==2 && sep==')') *last_space = 0;
      }
      std::strcpy(label,basename);
      cimglist_for(gmic_faves,l) {
        std::fprintf(file,"%s\n",gmic_faves[l].data());
        if (!std::strcmp(label,gmic_entries[indice_faves + l].data()))
          cimg_sprintf(label,"%s (%u)",basename.data(),++ind);
      }
      CImg<char> entry = gmic_entries[filter];
      for (char *p = std::strchr(label,'}'); p; p = std::strchr(p,'}')) *p = gmic_rbrace;  // Convert '}' if necessary.
      for (char *p = std::strchr(entry,'}'); p; p = std::strchr(p,'}')) *p = gmic_rbrace;
      std::fprintf(file,"{%s}{%s}{%s}{%s}",
                   label.data(),
                   entry.data(),
                   gmic_commands[filter].data(),
                   gmic_preview_commands[filter].data());
      const unsigned int nbp = get_filter_nbparams(filter);
      for (unsigned int n = 0; n<nbp; ++n) {
        CImg<char> param = get_filter_parameter(filter,n);
        set_filter_parameter(gmic_entries.size(),n,param);
        for (char *p = std::strchr(param,'}'); p; p = std::strchr(p,'}')) *p = gmic_rbrace; // Convert '}' if necessary.
        for (char *p = std::strchr(param,'\n'); p; p = std::strchr(p,'\n')) // Convert '\n' if necessary.
          *p = gmic_newline;
        std::fprintf(file,"{%s}",param.data());
      }
      std::fputc('\n',file);
      std::fclose(file);
      update_filters(false);
    } else if (get_verbosity_mode()>1)
      std::fprintf(cimg::output(),
                   "\n[gmic_gimp]./error/ Unable to write fave file '%s'.\n",
                   filename.data());
  }
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view),GTK_TREE_MODEL(tree_view_store));
  gimp_set_data("gmic_current_treepath","0",2);
  set_current_filter(0);
  flush_tree_view(tree_view);
  GtkTreePath *path = gtk_tree_path_new_from_string("0");
  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view),path,NULL,FALSE,0,0);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
  gtk_tree_selection_select_path(selection,path);
  gtk_tree_path_free(path);
  create_parameters_gui(false);
  _gimp_preview_invalidate();
}

void on_dialog_remove_fave_clicked(GtkWidget *const tree_view) {
  reset_button_parameters();
  const unsigned int filter = get_current_filter();
  gtk_widget_hide(relabel_hbox);
  gtk_widget_hide(fave_delete_button);
  if (filter) {
    CImg<char> filename(1024);
    cimg_snprintf(filename,filename.width(),"%sgimp_faves",
                  gmic::path_rc());
    std::FILE *file = std::fopen(filename,"wb");
    if (file) {
      const unsigned int _filter = filter - indice_faves;
      if (gmic_faves.size()==1) { std::fclose(file); std::remove(filename); }
      else {
        cimglist_for(gmic_faves,l) if (l!=(int)_filter) std::fprintf(file,"%s\n",gmic_faves[l].data());
        std::fclose(file);
        for (unsigned int i = filter; i<gmic_entries.size() - 1; ++i) { // Shift current parameters.
          const unsigned int nbp = get_filter_nbparams(i + 1);
          for (unsigned int n = 0; n<nbp; ++n)
            set_filter_parameter(i,n,get_filter_parameter(i + 1,n));
        }
      }
      update_filters(false);
    } else if (get_verbosity_mode()>1)
      std::fprintf(cimg::output(),
                   "\n[gmic_gimp]./error/ Unable to write fave file '%s'.\n",
                   filename.data());
  }
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view),GTK_TREE_MODEL(tree_view_store));
  gimp_set_data("gmic_current_treepath","0",2);
  set_current_filter(0);
  flush_tree_view(tree_view);
  GtkTreePath *path = gtk_tree_path_new_from_string("0");
  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view),path,NULL,FALSE,0,0);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
  gtk_tree_selection_select_path(selection,path);
  gtk_tree_path_free(path);
  create_parameters_gui(false);
  _gimp_preview_invalidate();
}

void on_dialog_refresh_clicked(GtkWidget *const tree_view) {
  reset_button_parameters();
  gtk_widget_hide(relabel_hbox);
  gtk_widget_hide(fave_delete_button);
  const unsigned int old_nb_filters = get_nbfilters();
  CImgList<char> invalid_servers = update_filters(get_net_update());
  if (invalid_servers) {
    if (get_verbosity_mode()>1) cimglist_for(invalid_servers,l) {
        std::fprintf(cimg::output(),
                     "\n[gmic_gimp]./update/ Unable to reach filters source '%s'.\n",
                     invalid_servers[l].data());
        std::fflush(cimg::output());
      }
    CImg<char>::string(t(0)).move_to(invalid_servers,0);
    cimglist_for(invalid_servers,l) {
      CImg<char>& server = invalid_servers[l];
      if (l) {
        server.resize(2 + server.width(),1,1,1,0,0,1);
        server[0] = '*';
        server[1] = ' ';
      }
      if (l!=invalid_servers.width() - 1) server.back() = '\n';
    }
    const CImg<char> error_message = invalid_servers>'x';

    GtkWidget *const
      message = gtk_message_dialog_new_with_markup(0,GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,
                                                   "%s",error_message.data());
    gtk_widget_show(message);
    gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);
  }

  const bool reset_params = gmic_entries.size()!=old_nb_filters;
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view),GTK_TREE_MODEL(tree_view_store));
  flush_tree_view(tree_view);
  create_parameters_gui(reset_params);
  _gimp_preview_invalidate();
}

void on_filter_selected(GtkWidget *const tree_view) {
  reset_button_parameters();
  GtkTreeSelection *const selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
  GtkTreeIter iter;
  GtkTreeModel *model;
  unsigned int filter = 0;
  if (gtk_tree_selection_get_selected(selection,&model,&iter)) {
    gtk_tree_model_get(model,&iter,0,&filter,-1);
    const char *const treepath = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(tree_view_store),&iter);
    gimp_set_data("gmic_current_treepath",treepath,std::strlen(treepath) + 1);
  }
  if (filter!=get_current_filter()) {
    gtk_widget_hide(relabel_hbox);
    gtk_widget_hide(fave_delete_button);
    set_current_filter(filter);
    create_parameters_gui(false);
    _create_dialog_gui = true;
    latest_preview_buffer.assign();
    _gimp_preview_invalidate();
  }
}

void _on_filter_doubleclicked(GtkWidget *const entry) {
  reset_button_parameters();
  const unsigned int filter = get_current_filter();
  cimg::unused(entry);
  gtk_widget_hide(relabel_hbox);
  gtk_widget_hide(fave_delete_button);
  if (filter>=indice_faves) {
    const char *const __label = gtk_entry_get_text(GTK_ENTRY(relabel_entry));
    char *const label = g_markup_escape_text(__label,std::strlen(__label));
    if (*label) {
      CImg<char> filename(1024);
      cimg_snprintf(filename,filename.width(),"%sgimp_faves",
                    gmic::path_rc());
      std::FILE *file = std::fopen(filename,"wb");
      if (file) {
        const unsigned int _filter = filter - indice_faves;
        cimglist_for(gmic_faves,l) if (l!=(int)_filter) std::fprintf(file,"%s\n",gmic_faves[l].data());
        else {
          CImg<char> _label = CImg<char>::string(label);
          for (char *p = std::strchr(_label,'}'); p; p = std::strchr(p,'}')) // Convert '}' if necessary.
            *p = gmic_rbrace;
          std::fprintf(file,"{%s%s\n",_label.data(),std::strchr(gmic_faves[l].data(),'}'));
        }
        std::fclose(file);
        update_filters(false);
      } else if (get_verbosity_mode()>1)
        std::fprintf(cimg::output(),
                     "\n[gmic_gimp]./error/ Unable to write fave file '%s'.\n",
                     filename.data());
      gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view),GTK_TREE_MODEL(tree_view_store));
      gimp_set_data("gmic_current_treepath","0",2);
      set_current_filter(0);
      flush_tree_view(tree_view);
      GtkTreePath *path = gtk_tree_path_new_from_string("0");
      gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view),path,NULL,FALSE,0,0);
      GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
      gtk_tree_selection_select_path(selection,path);
      gtk_tree_path_free(path);
      create_parameters_gui(false);
    }
    g_free(label);
  }
}

void on_filter_doubleclicked(GtkWidget *const tree_view) {
  reset_button_parameters();
  const unsigned int filter = get_current_filter();
  if (!filter) { // Expand/collapse folder.
    CImg<char> current_path(64); *current_path = 0;
    gimp_get_data("gmic_current_treepath",current_path);
    if (*current_path) {
      GtkTreePath *path = gtk_tree_path_new_from_string(current_path);
      const bool is_expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(tree_view),path);
      if (is_expanded) gtk_tree_view_collapse_row(GTK_TREE_VIEW(tree_view),path);
      else gtk_tree_view_expand_row(GTK_TREE_VIEW(tree_view),path,false);
      gtk_tree_path_free(path);
    }
  } else if (filter>=indice_faves) { // Rename fave filter.
    is_block_preview = true;
    gtk_label_set_markup(GTK_LABEL(markup2ascii),gmic_entries[filter].data());
    gtk_entry_set_text(GTK_ENTRY(relabel_entry),gtk_label_get_text(GTK_LABEL(markup2ascii)));
    gtk_widget_show(relabel_hbox);
    gtk_widget_grab_focus(relabel_entry);
  }
}

// Process the selected image/layers.
//------------------------------------
void process_image(const char *const command_line, const bool is_apply) {
  if (!gimp_image_is_valid(image_id)) return;
  const unsigned int
    filter = get_current_filter(),
    _output_mode = get_output_mode(),
    output_mode = get_input_mode()==0?std::max(1U,_output_mode):_output_mode,
    verbosity_mode = get_verbosity_mode();

  if (!command_line && !filter) return;
  bool update_parameters = false;
  const CImg<char> _command_line = command_line?CImg<char>::string(command_line):get_command_line(false);
  if (!_command_line || std::strstr(_command_line," -_none_")) return;

  // Reset values for button parameters.
  cimglist_for(gmic_button_parameters,l) set_filter_parameter(filter,gmic_button_parameters(l,0),"0");

  // Retrieve G'MIC command to apply.
  CImg<char> new_label(256), progress_label;
  *new_label = 0;
  if (run_mode!=GIMP_RUN_NONINTERACTIVE) {
    gtk_label_set_markup(GTK_LABEL(markup2ascii),gmic_entries[filter].data());
    CImg<char>::string(gtk_label_get_text(GTK_LABEL(markup2ascii))).move_to(progress_label);
    gimp_progress_init_printf(" G'MIC: %s...",progress_label.data());
    const char *const cl = _command_line.data() +
      (!std::strncmp(_command_line,"-v -99 ",7)?7:
       !std::strncmp(_command_line,"-v 0 ",5)?5:
       !std::strncmp(_command_line,"-debug ",7)?7:0);
    cimg_snprintf(new_label,new_label.width(),"[G'MIC] %s: %s",gtk_label_get_text(GTK_LABEL(markup2ascii)),cl);
    cimg::strellipsize(new_label,240,false);
  } else {
    cimg_snprintf(new_label,new_label.width(),"G'MIC: %s...",_command_line.data());
    cimg::strellipsize(new_label,240,false);
    gimp_progress_init_printf("%s",new_label.data());
    cimg_snprintf(new_label,new_label.width(),"[G'MIC]: %s",_command_line.data());
    cimg::strellipsize(new_label,240,false);
  }

  int image_nb_layers = 0, is_selection = 0, sel_x0 = 0, sel_y0 = 0, sel_x1 = 0, sel_y1 = 0;
  unsigned int image_width = 0, image_height = 0;  gimp_image_get_layers(image_id,&image_nb_layers);
  if (image_nb_layers) {
    image_width = gimp_image_width(image_id);
    image_height = gimp_image_height(image_id);
  }
  if (!gimp_selection_bounds(image_id,&is_selection,&sel_x0,&sel_y0,&sel_x1,&sel_y1)) is_selection = 0;
  else if (output_mode<1 || output_mode>2) sel_x0 = sel_y0 = 0;

  // Init structure used to pass processing arguments.
  st_process_thread spt;
  spt.is_preview = false;
  spt.command_line = _command_line;
  spt.verbosity_mode = get_verbosity_mode();
  spt.images_names.assign();
  spt.progress = -1;
  spt.set_env();

  // Get input layers for the chosen filter.
  const CImg<int> layers = get_input_layers(spt.images);
  CImg<int> layer_dimensions(spt.images.size(),4);
  CImg<char> layer_name(256);
  cimglist_for(spt.images,p) {
    const CImg<gmic_pixel_type>& img = spt.images[p];
    layer_dimensions(p,0) = img.width();
    layer_dimensions(p,1) = img.height();
    layer_dimensions(p,2) = img.depth();
    layer_dimensions(p,3) = img.spectrum();
    const GimpLayerModeEffects blendmode = gimp_layer_get_mode(layers[p]);
    const float opacity = gimp_layer_get_opacity(layers[p]);
    int posx = 0, posy = 0;
    if (is_selection && output_mode) {
      if (output_mode>=1 && output_mode<=2) { posx = sel_x0; posy = sel_y0; }
    } else gimp_drawable_offsets(layers[p],&posx,&posy);
    CImg<char> _layer_name = CImg<char>::string(gimp_item_get_name(layers[p]));
    cimg_for(_layer_name,pn,char) if (*pn=='(') *pn = 21; else if (*pn==')') *pn = 22;
    cimg_snprintf(layer_name,layer_name.width(),"mode(%s),opacity(%g),pos(%d,%d),name(%s)",
                  s_blendmode[blendmode],opacity,posx,posy,
                  _layer_name.data());
    CImg<char>::string(layer_name).move_to(spt.images_names);
  }

  // Create processing thread and wait for its completion.
  bool is_abort = spt.is_abort = false;
  if (run_mode!=GIMP_RUN_NONINTERACTIVE) {
    const cimg_ulong time0 = cimg::time();
    cimg::mutex(25); p_spt = (void*)&spt; cimg::mutex(25,0);
    spt.is_thread = true;
    pthread_mutex_init(&spt.is_running,0);
    pthread_mutex_init(&spt.wait_lock,0);
    pthread_cond_init(&spt.wait_cond,0);
    pthread_mutex_lock(&spt.wait_lock);

#if defined(__MACOSX__) || defined(__APPLE__)
    cimg::unused(time0);
    const cimg_ulong stacksize = (cimg_ulong)8*1024*1024;
    pthread_attr_t thread_attr;
    if (!pthread_attr_init(&thread_attr) && !pthread_attr_setstacksize(&thread_attr,stacksize))
      pthread_create(&spt.thread,&thread_attr,process_thread,(void*)&spt);
    else
#endif // #if defined(__MACOSX__) || defined(__APPLE__)
      pthread_create(&spt.thread,0,process_thread,(void*)&spt);

    pthread_cond_wait(&spt.wait_cond,&spt.wait_lock);  // Wait for the thread to lock the mutex.
    pthread_mutex_unlock(&spt.wait_lock);
    pthread_mutex_destroy(&spt.wait_lock);

    unsigned int i = 0, used_memory = 0;
    cimg::unused(i,used_memory);
    while (pthread_mutex_trylock(&spt.is_running)) {
      if (spt.progress>=0) gimp_progress_update(std::min(1.0,spt.progress/100.0));
      else gimp_progress_pulse();
      cimg::wait(333);

#if !defined(__MACOSX__) && !defined(__APPLE__)
      if (!(i%10)) { // Update memory usage.
        used_memory = 0;
#if cimg_OS==2
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(),&pmc,sizeof(pmc)))
          used_memory = (cimg_ulong)(pmc.WorkingSetSize/1024/1024);
#elif cimg_OS==1 // #if cimg_OS==2
        CImg<char> st; st.load_raw("/proc/self/status",512); st.back() = 0;
        const char *const s = std::strstr(st,"VmRSS:");
        if (s && cimg_sscanf(s + 7,"%u",&used_memory)==1) used_memory/=1024;
#endif // #if cimg_OS==2
      }

      if (!(i%3)) {
        const cimg_ulong t = (cimg::time() - time0)/1000;
        if (used_memory)
          gimp_progress_set_text_printf(spt.progress<0?
                                        " G'MIC: %s... [%lu second%s, %u Mb]":
                                        " G'MIC: %s... [%lu second%s, %u Mb]   %d%%",
                                        progress_label.data(),
                                        t,t!=1?"s":"",
                                        used_memory,(int)spt.progress);
        else
          gimp_progress_set_text_printf(spt.progress<0?
                                        " G'MIC: %s... [%lu second%s]":
                                        " G'MIC: %s... [%lu second%s]   %d%%",
                                        progress_label.data(),
                                        t,t!=1?"s":"",(int)spt.progress);
      }
      ++i;
#endif // #if !defined(__MACOSX__) && !defined(__APPLE__)
    }

    gimp_progress_update(1.0);
    pthread_join(spt.thread,0);
    pthread_mutex_unlock(&spt.is_running);
    pthread_mutex_destroy(&spt.is_running);
    is_abort = spt.is_abort;
    cimg::mutex(25); p_spt = (void*)0; cimg::mutex(25,0);
  } else {
    spt.is_thread = false;
    process_thread(&spt);
  }

  // Check that everything went fine, else display an error dialog.
  if (spt.error_message) {
    if (run_mode!=GIMP_RUN_NONINTERACTIVE) {
      GtkWidget *const
        message = gtk_message_dialog_new(0,GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,"%s",
                                         spt.error_message.data());
      gtk_widget_show(message);
      gtk_dialog_run(GTK_DIALOG(message));
      gtk_widget_destroy(message);
    } else {
      std::fprintf(cimg::output(),"\n[gmic_gimp]./error/ When running command '%s', this error occured:\n%s\n",
                   _command_line.data(),spt.error_message.data());
      std::fflush(cimg::output());
    }
    status = GIMP_PDB_CALLING_ERROR;
  } else if (!is_abort) {

    // Get output layers dimensions and check if input/output layers have compatible dimensions.
    unsigned int max_width = 0, max_height = 0, max_spectrum = 0;
    cimglist_for(spt.images,l) {
      if (spt.images[l].is_empty()) { spt.images.remove(l--); continue; } // Discard possible empty images.
      if (spt.images[l]._width>max_width) max_width = spt.images[l]._width;
      if (spt.images[l]._height>max_height) max_height = spt.images[l]._height;
      if (spt.images[l]._spectrum>max_spectrum) max_spectrum = spt.images[l]._spectrum;
    }
    bool is_compatible_dimensions = (spt.images.size()==layers._height);
    for (unsigned int p = 0; p<spt.images.size() && is_compatible_dimensions; ++p) {
      const CImg<gmic_pixel_type>& img = spt.images[p];
      const bool
        source_is_alpha = (layer_dimensions(p,3)==2 || layer_dimensions(p,3)>=4),
        dest_is_alpha = (img.spectrum()==2 || img.spectrum()>=4);
      if (dest_is_alpha && !source_is_alpha) { gimp_layer_add_alpha(layers[p]); ++layer_dimensions(p,3); }
      if (img.width()!=layer_dimensions(p,0) ||
          img.height()!=layer_dimensions(p,1)) is_compatible_dimensions = false;
    }

    // Transfer the output layers back into GIMP.
    GimpLayerModeEffects layer_blendmode = GIMP_NORMAL_MODE;
    gint layer_posx = 0, layer_posy = 0;
    double layer_opacity = 100;
    CImg<char> layer_name;

    switch (output_mode) {
    case 0 : { // Output in 'Replace' mode.
      gint rgn_x, rgn_y, rgn_width, rgn_height;
      gimp_image_undo_group_start(image_id);
      if (is_compatible_dimensions) cimglist_for(spt.images,p) { // Direct replacement of the layer data.
          layer_blendmode = gimp_layer_get_mode(layers[p]);
          layer_opacity = gimp_layer_get_opacity(layers[p]);
          gimp_drawable_offsets(layers[p],&layer_posx,&layer_posy);
          CImg<char>::string(gimp_item_get_name(layers[p])).move_to(layer_name);
          get_output_layer_props(spt.images_names[p],layer_blendmode,layer_opacity,layer_posx,layer_posy,layer_name);
          CImg<gmic_pixel_type> &img = spt.images[p];
          calibrate_image(img,layer_dimensions(p,3),false);
          if (gimp_drawable_mask_intersect(layers[p],&rgn_x,&rgn_y,&rgn_width,&rgn_height)) {

#if GIMP_MINOR_VERSION<=8
            GimpDrawable *drawable = gimp_drawable_get(layers[p]);
            GimpPixelRgn region;
            gimp_pixel_rgn_init(&region,drawable,rgn_x,rgn_y,rgn_width,rgn_height,true,true);
            convert_image2uchar(img);
            gimp_pixel_rgn_set_rect(&region,(guchar*)img.data(),rgn_x,rgn_y,rgn_width,rgn_height);
            gimp_drawable_flush(drawable);
            gimp_drawable_merge_shadow(layers[p],true);
            gimp_drawable_update(layers[p],rgn_x,rgn_y,rgn_width,rgn_height);
            gimp_drawable_detach(drawable);
#else
            GeglRectangle rect;
            gegl_rectangle_set(&rect,rgn_x,rgn_y,rgn_width,rgn_height);
            GeglBuffer *buffer = gimp_drawable_get_shadow_buffer(layers[p]);
            const char *const format = img.spectrum()==1?"Y' float":img.spectrum()==2?"Y'A float":
              img.spectrum()==3?"R'G'B' float":"R'G'B'A float";
            (img/=255).permute_axes("cxyz");
            gegl_buffer_set(buffer,&rect,0,babl_format(format),img.data(),0);
            g_object_unref(buffer);
            gimp_drawable_merge_shadow(layers[p],true);
            gimp_drawable_update(layers[p],0,0,img.width(),img.height());
#endif
            gimp_layer_set_mode(layers[p],layer_blendmode);
            gimp_layer_set_opacity(layers[p],layer_opacity);
            gimp_layer_set_offsets(layers[p],layer_posx,layer_posy);
            if (verbosity_mode==1) gimp_item_set_name(layers[p],new_label);
            else if (layer_name) gimp_item_set_name(layers[p],layer_name);
          }
          img.assign();

        } else { // Indirect replacement: create new layers.
          gimp_selection_none(image_id);
          const int layer_pos = _gimp_image_get_item_position(image_id,layers[0]);
          max_width = max_height = 0;
          cimglist_for(spt.images,p) if (spt.images[p]) {
            layer_posx = layer_posy = 0;
            if (p<layers.height()) {
              layer_blendmode = gimp_layer_get_mode(layers[p]);
              layer_opacity = gimp_layer_get_opacity(layers[p]);
              if (!is_selection) gimp_drawable_offsets(layers[p],&layer_posx,&layer_posy);
              CImg<char>::string(gimp_item_get_name(layers[p])).move_to(layer_name);
              gimp_image_remove_layer(image_id,layers[p]);
            } else {
              layer_blendmode = GIMP_NORMAL_MODE;
              layer_opacity = 100;
              layer_name.assign();
            }
            get_output_layer_props(spt.images_names[p],layer_blendmode,layer_opacity,layer_posx,layer_posy,layer_name);
            if (layer_posx + spt.images[p]._width>max_width) max_width = layer_posx + spt.images[p]._width;
            if (layer_posy + spt.images[p]._height>max_height) max_height = layer_posy + spt.images[p]._height;

            CImg<gmic_pixel_type> &img = spt.images[p];
            if (gimp_image_base_type(image_id)==GIMP_GRAY)
              calibrate_image(img,(img.spectrum()==1 || img.spectrum()==3)?1:2,false);
            else calibrate_image(img,(img.spectrum()==1 || img.spectrum()==3)?3:4,false);
            gint layer_id = gimp_layer_new(image_id,new_label,img.width(),img.height(),
                                           img.spectrum()==1?GIMP_GRAY_IMAGE:
                                           img.spectrum()==2?GIMP_GRAYA_IMAGE:
                                           img.spectrum()==3?GIMP_RGB_IMAGE:GIMP_RGBA_IMAGE,
                                           layer_opacity,layer_blendmode);
            gimp_layer_set_offsets(layer_id,layer_posx,layer_posy);
            if (verbosity_mode==1) gimp_item_set_name(layer_id,new_label);
            else if (layer_name) gimp_item_set_name(layer_id,layer_name);
            gimp_image_insert_layer(image_id,layer_id,-1,layer_pos + p);

#if GIMP_MINOR_VERSION<=8
            GimpDrawable *drawable = gimp_drawable_get(layer_id);
            GimpPixelRgn region;
            gimp_pixel_rgn_init(&region,drawable,0,0,drawable->width,drawable->height,true,true);
            convert_image2uchar(img);
            gimp_pixel_rgn_set_rect(&region,(guchar*)img.data(),0,0,img.width(),img.height());
            gimp_drawable_flush(drawable);
            gimp_drawable_merge_shadow(layer_id,true);
            gimp_drawable_update(layer_id,0,0,drawable->width,drawable->height);
            gimp_drawable_detach(drawable);
#else
            GeglBuffer *buffer = gimp_drawable_get_shadow_buffer(layer_id);
            const char *const format = img.spectrum()==1?"Y' float":img.spectrum()==2?"Y'A float":
              img.spectrum()==3?"R'G'B' float":"R'G'B'A float";
            (img/=255).permute_axes("cxyz");
            gegl_buffer_set(buffer,NULL,0,babl_format(format),img.data(),0);
            g_object_unref(buffer);
            gimp_drawable_merge_shadow(layer_id,true);
            gimp_drawable_update(layer_id,0,0,img.width(),img.height());
#endif
            img.assign();
          }

          for (unsigned int p = spt.images._width; p<layers._height; ++p) gimp_image_remove_layer(image_id,layers[p]);
          if (image_nb_layers==layers.height()) gimp_image_resize(image_id,max_width,max_height,0,0);
          else gimp_image_resize(image_id,std::max(image_width,max_width),std::max(image_height,max_height),0,0);

      }
      gimp_image_undo_group_end(image_id);
    } break;

    case 1 : case 2 : { // Output in 'New [active] layer(s)' mode.
      const gint active_layer_id = gimp_image_get_active_layer(image_id);
      if (active_layer_id>=0) {
        gimp_image_undo_group_start(image_id);
        gint top_layer_id = 0, layer_id = 0;
        max_width = max_height = 0;
        cimglist_for(spt.images,p) if (spt.images[p]) {
          layer_blendmode = GIMP_NORMAL_MODE;
          layer_opacity = 100;
          layer_posx = layer_posy = 0;
          if (layers.height()==1) {
            if (!is_selection) gimp_drawable_offsets(active_layer_id,&layer_posx,&layer_posy);
            CImg<char>::string(gimp_item_get_name(active_layer_id)).move_to(layer_name);
          } else layer_name.assign();
          get_output_layer_props(spt.images_names[p],layer_blendmode,layer_opacity,layer_posx,layer_posy,layer_name);
          if (layer_posx + spt.images[p]._width>max_width) max_width = layer_posx + spt.images[p]._width;
          if (layer_posy + spt.images[p]._height>max_height) max_height = layer_posy + spt.images[p]._height;

          CImg<gmic_pixel_type> &img = spt.images[p];
          if (gimp_image_base_type(image_id)==GIMP_GRAY)
            calibrate_image(img,!is_selection && (img.spectrum()==1 || img.spectrum()==3)?1:2,false);
          else
            calibrate_image(img,!is_selection && (img.spectrum()==1 || img.spectrum()==3)?3:4,false);
          layer_id = gimp_layer_new(image_id,new_label,img.width(),img.height(),
                                    img.spectrum()==1?GIMP_GRAY_IMAGE:
                                    img.spectrum()==2?GIMP_GRAYA_IMAGE:
                                    img.spectrum()==3?GIMP_RGB_IMAGE:GIMP_RGBA_IMAGE,
                                    layer_opacity,layer_blendmode);
          if (!p) top_layer_id = layer_id;
          gimp_layer_set_offsets(layer_id,layer_posx,layer_posy);
          if (verbosity_mode==1) gimp_item_set_name(layer_id,new_label);
          else if (layer_name) gimp_item_set_name(layer_id,layer_name);
          gimp_image_insert_layer(image_id,layer_id,-1,p);

#if GIMP_MINOR_VERSION<=8
          GimpDrawable *drawable = gimp_drawable_get(layer_id);
          GimpPixelRgn region;
          gimp_pixel_rgn_init(&region,drawable,0,0,drawable->width,drawable->height,true,true);
          convert_image2uchar(img);
          gimp_pixel_rgn_set_rect(&region,(guchar*)img.data(),0,0,img.width(),img.height());
          gimp_drawable_flush(drawable);
          gimp_drawable_merge_shadow(layer_id,true);
          gimp_drawable_update(layer_id,0,0,drawable->width,drawable->height);
          gimp_drawable_detach(drawable);
#else
          GeglBuffer *buffer = gimp_drawable_get_shadow_buffer(layer_id);
          const char *const format = img.spectrum()==1?"Y' float":img.spectrum()==2?"Y'A float":
            img.spectrum()==3?"R'G'B' float":"R'G'B'A float";
          (img/=255).permute_axes("cxyz");
          gegl_buffer_set(buffer,NULL,0,babl_format(format),img.data(),0);
          g_object_unref(buffer);
          gimp_drawable_merge_shadow(layer_id,true);
          gimp_drawable_update(layer_id,0,0,img.width(),img.height());
#endif
          img.assign();
        }
        const unsigned int Mw = std::max(image_width,max_width), Mh = std::max(image_height,max_height);
        if (Mw && Mh) gimp_image_resize(image_id,Mw,Mh,0,0);
        if (output_mode==1) gimp_image_set_active_layer(image_id,active_layer_id);
        else { // Destroy preview widget will force the preview to get the new active layer as base image.
          gimp_image_set_active_layer(image_id,top_layer_id);
          if (is_apply) {
            if (gui_preview && GTK_IS_WIDGET(gui_preview)) gtk_widget_destroy(gui_preview);
            gui_preview = 0;
          }
        }
        gimp_image_undo_group_end(image_id);
      }
    } break;

    default : { // Output in 'New image' mode.
      if (spt.images.size()) {
        const gint active_layer_id = gimp_image_get_active_layer(image_id);
        if (active_layer_id>=0) {
#if GIMP_MINOR_VERSION<=8
          const int nimage_id = gimp_image_new(max_width,max_height,max_spectrum<=2?GIMP_GRAY:GIMP_RGB);
#else
          const int nimage_id = gimp_image_new_with_precision(max_width,max_height,max_spectrum<=2?GIMP_GRAY:GIMP_RGB,
                                                              gimp_image_get_precision(image_id));
#endif
          gimp_image_undo_group_start(nimage_id);
          cimglist_for(spt.images,p) if (spt.images[p]) {
            layer_blendmode = GIMP_NORMAL_MODE;
            layer_opacity = 100;
            layer_posx = layer_posy = 0;
            if (layers.height()==1) {
              if (!is_selection) gimp_drawable_offsets(active_layer_id,&layer_posx,&layer_posy);
              CImg<char>::string(gimp_item_get_name(active_layer_id)).move_to(layer_name);
            } else layer_name.assign();
            get_output_layer_props(spt.images_names[p],layer_blendmode,layer_opacity,layer_posx,layer_posy,layer_name);

            CImg<gmic_pixel_type> &img = spt.images[p];
            if (gimp_image_base_type(nimage_id)!=GIMP_GRAY)
              calibrate_image(img,(img.spectrum()==1 || img.spectrum()==3)?3:4,false);
            gint layer_id = gimp_layer_new(nimage_id,new_label,img.width(),img.height(),
                                           img.spectrum()==1?GIMP_GRAY_IMAGE:
                                           img.spectrum()==2?GIMP_GRAYA_IMAGE:
                                           img.spectrum()==3?GIMP_RGB_IMAGE:GIMP_RGBA_IMAGE,
                                           layer_opacity,layer_blendmode);
            gimp_layer_set_offsets(layer_id,layer_posx,layer_posy);
            if (verbosity_mode==1) gimp_item_set_name(layer_id,new_label);
            else if (layer_name) gimp_item_set_name(layer_id,layer_name);
            gimp_image_insert_layer(nimage_id,layer_id,-1,p);

#if GIMP_MINOR_VERSION<=8
            GimpDrawable *drawable = gimp_drawable_get(layer_id);
            GimpPixelRgn region;
            gimp_pixel_rgn_init(&region,drawable,0,0,drawable->width,drawable->height,true,true);
            convert_image2uchar(img);
            gimp_pixel_rgn_set_rect(&region,(guchar*)img.data(),0,0,img.width(),img.height());
            gimp_drawable_flush(drawable);
            gimp_drawable_merge_shadow(layer_id,true);
            gimp_drawable_update(layer_id,0,0,drawable->width,drawable->height);
            gimp_drawable_detach(drawable);
#else
            GeglBuffer *buffer = gimp_drawable_get_shadow_buffer(layer_id);
            const char *const format = img.spectrum()==1?"Y' float":img.spectrum()==2?"Y'A float":
              img.spectrum()==3?"R'G'B' float":"R'G'B'A float";
            (img/=255).permute_axes("cxyz");
            gegl_buffer_set(buffer,NULL,0,babl_format(format),img.data(),0);
            g_object_unref(buffer);
            gimp_drawable_merge_shadow(layer_id,true);
            gimp_drawable_update(layer_id,0,0,img.width(),img.height());
#endif
            img.assign();
          }
          gimp_display_new(nimage_id);
          gimp_image_undo_group_end(nimage_id);
        }
      }
    }
    }

    // Update values of parameters if invoked command requests it (using status value).
    const unsigned int l_status = spt.status._width;
    if (l_status>3 && spt.status[0]==gmic_lbrace && spt.status[l_status - 2]==gmic_rbrace) {
      spt.status.crop(1,l_status - 3);
      CImgList<char> return_values = spt.status.get_split(CImg<char>::vector(gmic_rbrace,gmic_lbrace),false,false);
      if (return_values._width==get_filter_nbparams(filter))
        cimglist_for(return_values,l) {
          gmic::strreplace_fw(return_values[l].resize(1,return_values[l].height() + 1,1,1,0));
          if (std::strcmp(return_values[l].data(),get_filter_parameter(filter,l))) {
            set_filter_parameter(filter,l,return_values[l]);
            update_parameters = true;
          }
        }
    }
  }

  if (run_mode!=GIMP_RUN_NONINTERACTIVE) {
    gimp_progress_end();
    gimp_displays_flush();
    if (is_apply) _gimp_preview_invalidate();
    if (update_parameters && is_apply) {
      const bool pstate = gimp_preview_get_update(GIMP_PREVIEW(gui_preview));
      gimp_preview_set_update(GIMP_PREVIEW(gui_preview),false);
      create_parameters_gui(false);
      gimp_preview_set_update(GIMP_PREVIEW(gui_preview),pstate);
    }
  }
}

// Process the preview image.
//---------------------------
void process_preview() {
  if (is_block_preview) { is_block_preview = false; return; }
  if (!gimp_image_is_valid(image_id)) return;
  preview_with_icc();
  const unsigned int filter = get_current_filter();
  if (!filter) return;
  const CImg<char> command_line = get_command_line(true);
  if (!command_line || std::strstr(command_line," -_none_")) return;

#if GIMP_MINOR_VERSION>8
  img_profile = gimp_image_get_effective_color_profile(image_id);
#endif

  bool update_parameters = false;
  int wp, hp, sp, xp, yp;
  static int _xp = -1, _yp = -1;
  static double _factor = -1;
  guchar *const ptr0 = gimp_zoom_preview_get_source(GIMP_ZOOM_PREVIEW(gui_preview),&wp,&hp,&sp);
  const double factor = gimp_zoom_preview_get_factor(GIMP_ZOOM_PREVIEW(gui_preview));
  gimp_preview_get_position(GIMP_PREVIEW(gui_preview),&xp,&yp);
  if (xp!=_xp || _yp!=yp || _factor!=factor) {
    _xp = xp; _yp = yp; _factor = factor;
    computed_preview.assign();
    latest_preview_buffer.assign();
  }

  if (!computed_preview) {

    // Get input layers for the chosen filter and convert then to the preview size if necessary.
    st_process_thread spt;
    spt.is_preview = true;
    spt.is_thread = false;
    spt.command_line = command_line;
    spt.verbosity_mode = get_verbosity_mode();
    spt.progress = -1;
    spt.set_env();

    CImg<char> layer_name(256);
    const unsigned int input_mode = get_input_mode();
    int nb_layers = 0, *const layers = gimp_image_get_layers(image_id,&nb_layers);

    if (nb_layers && input_mode) {
      if (!preview_image_id &&
          (input_mode==1 ||
           (input_mode==2 && nb_layers==1) ||
           (input_mode==3 && nb_layers==1 && _gimp_item_get_visible(*layers)) ||
           (input_mode==4 && nb_layers==1 && !_gimp_item_get_visible(*layers)) ||
           (input_mode==5 && nb_layers==1))) {

        // Single input layer: get the default thumbnail provided by GIMP.
        spt.images.assign(1);
        spt.images_names.assign(1);
        spt.images[0].assign(ptr0,sp,wp,hp).permute_axes("yzcx");

        const float opacity = gimp_layer_get_opacity(*layers);
        const GimpLayerModeEffects blendmode = gimp_layer_get_mode(*layers);
        int posx = 0, posy = 0;
        gimp_drawable_offsets(*layers,&posx,&posy);
        const int
          w = gimp_drawable_width(*layers),
          h = gimp_drawable_height(*layers),
          ox = (int)(posx*wp/w),
          oy = (int)(posy*hp/h);
        CImg<char> _layer_name = CImg<char>::string(gimp_item_get_name(*layers));
        cimg_for(_layer_name,p,char) if (*p=='(') *p = '['; else if (*p==')') *p = ']';
        cimg_snprintf(layer_name,layer_name.width(),"mode(%s),opacity(%g),pos(%d,%d),name(%s)",
                      s_blendmode[blendmode],opacity,ox,oy,
                      _layer_name.data());
        CImg<char>::string(layer_name).move_to(spt.images_names[0]);
      } else {

        // Multiple input layers: compute a 'hand-made' set of thumbnails.
        CImgList<unsigned char> images_uchar;
        const CImg<int> layers = get_input_layers(images_uchar);
        if (images_uchar) {
          spt.images.assign(images_uchar.size());
          spt.images_names.assign(images_uchar.size());

          // Retrieve global preview ratio.
          const int
            active_layer = gimp_image_get_active_layer(image_id),
            w0 = gimp_drawable_width(active_layer),
            h0 = gimp_drawable_height(active_layer),
            _wp = (int)cimg::round(wp/preview_image_factor),
            _hp = (int)cimg::round(hp/preview_image_factor);
          const double ratio = std::max((double)_wp/w0,(double)_hp/h0);

          // Retrieve resized and cropped preview layers.
          cimg_forY(layers,p) {
            const float opacity = gimp_layer_get_opacity(layers[p]);
            const GimpLayerModeEffects blendmode = gimp_layer_get_mode(layers[p]);
            int posx = 0, posy = 0;
            gimp_drawable_offsets(layers[p],&posx,&posy);

            CImg<unsigned char>& img = images_uchar[p];
            const int
              w = img.width(),
              h = img.height(),
              x0 = (int)(xp*w/wp/factor),
              y0 = (int)(yp*h/hp/factor),
              x1 = x0 + (int)(w/factor) - 1,
              y1 = y0 + (int)(h/factor) - 1,
              ox = (int)(posx*_wp/w0),
              oy = (int)(posy*_hp/h0);
            img.get_crop(x0,y0,x1,y1).resize(cimg::round(img.width()*ratio),
                                             cimg::round(img.height()*ratio)).
              move_to(spt.images[p]);
            cimg_snprintf(layer_name,layer_name.width(),"mode(%s),opacity(%g),pos(%d,%d)",
                          s_blendmode[blendmode],opacity,ox,oy);
            CImg<char>::string(layer_name).move_to(spt.images_names[p]);
          }
        }
      }
    }

    // Run G'MIC.
    CImg<unsigned char> original_preview;
    CImg<char> progress_label;
    if (spt.images) original_preview = spt.images[0];
    else original_preview.assign(wp,hp,1,4,0);

    bool is_abort = spt.is_abort = false;
    cimg::mutex(25); p_spt = (void*)&spt; cimg::mutex(25,0);
    spt.is_thread = true;
    pthread_mutex_init(&spt.is_running,0);
    pthread_mutex_init(&spt.wait_lock,0);
    pthread_cond_init(&spt.wait_cond,0);
    pthread_mutex_lock(&spt.wait_lock);

#if defined(__MACOSX__) || defined(__APPLE__)
    const cimg_ulong stacksize = (cimg_ulong)8*1024*1024;
    pthread_attr_t thread_attr;
    if (!pthread_attr_init(&thread_attr) && !pthread_attr_setstacksize(&thread_attr,stacksize))
      // Reserve enough stack size for the new thread.
      pthread_create(&spt.thread,&thread_attr,process_thread,(void*)&spt);
    else
#endif // #if defined(__MACOSX__) || defined(__APPLE__)
      pthread_create(&spt.thread,0,process_thread,(void*)&spt);

    pthread_cond_wait(&spt.wait_cond,&spt.wait_lock); // Wait for the thread to lock the mutex.
    pthread_mutex_unlock(&spt.wait_lock);
    pthread_mutex_destroy(&spt.wait_lock);
    if (latest_preview_buffer.width()==wp && latest_preview_buffer.height()==hp && // Avoid preview flickering effect.
        latest_preview_buffer.spectrum()==sp)
      gimp_preview_draw_buffer(GIMP_PREVIEW(gui_preview),latest_preview_buffer.data(),wp*sp);
    while (pthread_mutex_trylock(&spt.is_running)) { // Loop that allows to get a responsive interface.
      while (gtk_events_pending()) { gtk_main_iteration(); }
      cimg::wait(333);
    }

    pthread_join(spt.thread,0);
    pthread_mutex_unlock(&spt.is_running);
    pthread_mutex_destroy(&spt.is_running);
    is_abort = spt.is_abort;
    cimg::mutex(25); p_spt = (void*)0; cimg::mutex(25,0);
    if (is_abort) return;

    // Manage possible errors.
    if (spt.error_message) {
      spt.images.assign();
      spt.images_names.assign();
      original_preview.move_to(spt.images);
      CImg<char> command(1024);
      cimg_snprintf(command,command.width(),"%s-gimp_error_preview \"%s\"",
                    get_verbosity_mode()>5?"-debug ":get_verbosity_mode()>3?"":"-v -99 ",
                    spt.error_message.data());
      try {
        gmic(command,spt.images,spt.images_names,gmic_additional_commands,true);

      } catch (...) {  // Fallback for '-gimp_error_preview'.
        const unsigned char white[] = { 155,155,155 };
        spt.images.assign(1).back().draw_text(0,0," Preview\n  error ",white,0,1,57).
          resize(-100,-100,1,4).get_shared_channel(3).dilate(5);
        spt.images[0].resize(wp,hp,1,4,0,0,0.5,0.5)+=100;
      }
      spt.status.assign();
    }

    // Transfer the output layers back into GIMP preview.
    CImgList<gmic_pixel_type> preview_images;

    switch (get_preview_mode()) {
    case 0 : // Preview 1st layer
      if (spt.images && spt.images.size()>0) spt.images[0].move_to(preview_images);
      break;
    case 1 : // Preview 2nd layer
      if (spt.images && spt.images.size()>1) spt.images[1].move_to(preview_images);
      break;
    case 2 : // Preview 3rd layer
      if (spt.images && spt.images.size()>2) spt.images[2].move_to(preview_images);
      break;
    case 3 : // Preview 4th layer
      if (spt.images && spt.images.size()>3) spt.images[3].move_to(preview_images);
      break;
    case 4 : { // Preview 1st->2nd layers
      cimglist_for_in(spt.images,0,1,l) spt.images[l].move_to(preview_images);
    } break;
    case 5 : { // Preview 1st->3nd layers
      cimglist_for_in(spt.images,0,2,l) spt.images[l].move_to(preview_images);
    } break;
    case 6 : { // Preview 1st->4nd layers
      cimglist_for_in(spt.images,0,3,l) spt.images[l].move_to(preview_images);
    } break;
    default : // Preview all layers
      spt.images.move_to(preview_images);
    }
    spt.images.assign();
    spt.images_names.assign();

    // Generate a single image representation from all output preview layers.
    unsigned int _sp = 0;
    cimglist_for(preview_images,l) if (preview_images[l]._spectrum>_sp) _sp = preview_images[l]._spectrum;
    if (_sp==1 || _sp==3) ++_sp;
    cimglist_for(preview_images,l) calibrate_image(preview_images[l],_sp,true);

    computed_preview.assign();
    if (preview_images.size()==1) preview_images[0].move_to(computed_preview);
    else if (preview_images.size()>1) try {
        CImgList<char> preview_images_names;
        gmic("-gimp_preview",preview_images,preview_images_names,gmic_additional_commands,true);
        if (preview_images.size()) preview_images[0].move_to(computed_preview);
        preview_images.assign();
        preview_images_names.assign();
      } catch (...) { // Fallback for '-gimp_preview'.
        const unsigned char white[] = { 155,155,155 };
        preview_images.assign();
        computed_preview.draw_text(0,0," Preview\n  error ",white,0,1,57).
          resize(-100,-100,1,4).get_shared_channel(3).dilate(5);
        computed_preview.resize(wp,hp,1,4,0,0,0.5,0.5)+=100;
      }
    if (!computed_preview) computed_preview.assign(wp,hp,1,4,0);
    if (computed_preview.width()!=wp || computed_preview.height()!=hp) {
      const double ratio = std::min((double)wp/computed_preview.width(),
                                    (double)hp/computed_preview.height());
      computed_preview.resize((int)cimg::round(computed_preview.width()*ratio),
                              (int)cimg::round(computed_preview.height()*ratio),
                              1,-100,2);
    }

    if (computed_preview.width()!=wp || computed_preview.height()!=hp)
      computed_preview.resize(wp,hp,1,-100,0,0,0.5,0.5);

    apply_icc(computed_preview);
    calibrate_image(computed_preview,sp,true);
    convert_image2uchar(computed_preview);
    computed_preview.channel(0);

    // Update values of parameters if invoked command requests it (using status value).
    const unsigned int l_status = spt.status._width;
    if (l_status>3 && spt.status[0]==gmic_lbrace && spt.status[l_status - 2]==gmic_rbrace) {
      spt.status.crop(1,l_status - 3);
      CImgList<char> return_values = spt.status.get_split(CImg<char>::vector(gmic_rbrace,gmic_lbrace),false,false);
      if (return_values._width==get_filter_nbparams(filter))
        cimglist_for(return_values,l) {
          gmic::strreplace_fw(return_values[l].resize(1,return_values[l].height() + 1,1,1,0));
          if (std::strcmp(return_values[l].data(),get_filter_parameter(filter,l))) {
            set_filter_parameter(filter,l,return_values[l]);
            update_parameters = true;
          }
        }
    }
  }

  // Display warning message about preview inaccuracy, if needed.
  double default_factor = gmic_preview_factors(filter,0);
  if (!default_factor) {
    const float
      pw = (float)gimp_preview_width,
      ph = (float)gimp_preview_height;
#if GIMP_MINOR_VERSION<=8
    const float
      dw = (float)gimp_zoom_preview_get_drawable(GIMP_ZOOM_PREVIEW(gui_preview))->width,
      dh = (float)gimp_zoom_preview_get_drawable(GIMP_ZOOM_PREVIEW(gui_preview))->height;
#else
    const int preview_drawable_id = gimp_zoom_preview_get_drawable_id(GIMP_ZOOM_PREVIEW(gui_preview));
    const float
      dw = (float)gimp_drawable_width(preview_drawable_id),
      dh = (float)gimp_drawable_height(preview_drawable_id);
#endif
    default_factor = std::sqrt((dw*dw + dh*dh)/(pw*pw + ph*ph));
  }
  const bool is_accurate_when_zoomed = gmic_preview_factors(filter,1);
  if (default_factor<0 || is_accurate_when_zoomed || cimg::abs(factor-default_factor)<0.1)
    gtk_widget_hide(gui_preview_warning);
  else gtk_widget_show(gui_preview_warning);

  // Update rendered image in preview widget.
  std::memcpy(ptr0,computed_preview.data(),wp*hp*sp*sizeof(unsigned char));
  gimp_preview_draw_buffer(GIMP_PREVIEW(gui_preview),ptr0,wp*sp);
  latest_preview_buffer.assign(ptr0,wp,hp,1,sp);
  g_free(ptr0);
  if (update_parameters) {
    const bool pstate = gimp_preview_get_update(GIMP_PREVIEW(gui_preview));
    gimp_preview_set_update(GIMP_PREVIEW(gui_preview),false);
    create_parameters_gui(false);
    gimp_preview_set_update(GIMP_PREVIEW(gui_preview),pstate);
  }
}

// Create the parameters GUI for the chosen filter.
//--------------------------------------------------
void create_parameters_gui(const bool reset_params) {
  const unsigned int filter = get_current_filter();
  gmic_button_parameters.assign();

  // Remove existing table in the parameters frame if necessary.
  GtkWidget *const child = GTK_WIDGET(gtk_bin_get_child(GTK_BIN(right_frame)));
  if (child) gtk_container_remove(GTK_CONTAINER(right_frame),child);

  // Create new table for the parameters frame.
  GtkWidget *table = 0;
  if (!filter) {  // No filter selected -> 1x1 table with default message.
    table = gtk_table_new(1,1,false);
    gtk_widget_show(table);
    GtkWidget *const label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),t("<i>Select a filter...</i>"));
    gtk_widget_show(label);
    gtk_table_attach(GTK_TABLE(table),label,0,1,0,1,
                     (GtkAttachOptions)(GTK_EXPAND),(GtkAttachOptions)(GTK_EXPAND),0,0);
    gtk_misc_set_alignment (GTK_MISC(label),0,0.5);
    gtk_frame_set_label(GTK_FRAME(right_frame),NULL);
  } else { // Filter selected -> Build the table for setting the parameters.
    CImg<char> s_label(256);
    cimg_snprintf(s_label,s_label.width(),"<b>  %s  </b>",gmic_entries[filter].data());
    GtkWidget *const frame_title = gtk_label_new(NULL);
    gtk_widget_show(frame_title);
    gtk_label_set_markup(GTK_LABEL(frame_title),s_label);
    gtk_frame_set_label_widget(GTK_FRAME(right_frame),frame_title);

    // Count number of filter arguments.
    CImg<char> argument_name(256), _argument_type(32), argument_arg(65536);
    *argument_name = *_argument_type = *argument_arg = 0;
    unsigned int nb_arguments = 0;
    for (const char *argument = gmic_arguments[filter].data(); *argument; ) {
      int err = cimg_sscanf(argument,"%255[^=]=%31[ a-zA-z](%65535[^)]",
                            argument_name.data(),_argument_type.data(),&(argument_arg[0]=0));
      if (err!=3) err = cimg_sscanf(argument,"%255[^=]=%31[ a-zA-z]{%65535[^}]",
                                    argument_name.data(),_argument_type.data(),argument_arg.data());
      if (err!=3) err = cimg_sscanf(argument,"%255[^=]=%31[ a-zA-z][%65535[^]]",
                                    argument_name.data(),_argument_type.data(),argument_arg.data());
      if (err>=2) {
        argument += std::strlen(argument_name) + std::strlen(_argument_type) + std::strlen(argument_arg) + 3;
        if (*argument) ++argument;
        ++nb_arguments;
      } else break;
    }

    unsigned int current_table_line = 0;
    if (!nb_arguments) { // Filter requires no parameters -> 1x1 table with default message.
      table = gtk_table_new(1,1,false);
      gtk_widget_show(table);
      GtkWidget *label = gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(label),t("<i>No parameters to set...</i>"));
      gtk_widget_show(label);
      gtk_table_attach(GTK_TABLE(table),label,0,1,0,1,
                       (GtkAttachOptions)(GTK_EXPAND),(GtkAttachOptions)(GTK_EXPAND),0,0);
      gtk_misc_set_alignment(GTK_MISC(label),0,0.5);
      if (event_infos) delete[] event_infos;
      event_infos = 0;
      set_filter_nbparams(filter,0);

    } else { // Filter requires parameters -> Create parameters table.

      // Create new table for putting parameters inside.
      table = gtk_table_new(nb_arguments,3,false);
      gtk_widget_show(table);
      gtk_table_set_row_spacings(GTK_TABLE(table),6);
      gtk_table_set_col_spacings(GTK_TABLE(table),6);
      gtk_container_set_border_width(GTK_CONTAINER(table),8);

      // Parse arguments list and add recognized one to the table.
      if (event_infos) delete[] event_infos;
      event_infos = new void*[2*nb_arguments];
      unsigned int current_argument = 0;
      const bool is_fave = filter>=indice_faves;
      for (const char *argument = gmic_arguments[filter].data(); *argument; ) {
        int err = cimg_sscanf(argument,"%255[^=]=%31[ a-zA-Z_](%65535[^)]",
                              argument_name.data(),_argument_type.data(),&(argument_arg[0]=0));
        if (err!=3) err = cimg_sscanf(argument,"%255[^=]=%31[ a-zA-Z_][%65535[^]]",
                                      argument_name.data(),_argument_type.data(),argument_arg.data());
        if (err!=3) err = cimg_sscanf(argument,"%255[^=]=%31[ a-zA-Z_]{%65535[^}]",
                                      argument_name.data(),_argument_type.data(),argument_arg.data());
        if (err>=2) {
          argument += std::strlen(argument_name) + std::strlen(_argument_type) + std::strlen(argument_arg) + 3;
          if (*argument) ++argument;
          cimg::strpare(argument_name,' ',false,true);
          cimg::strpare(argument_name,'\"',true,false);
          cimg::strunescape(argument_name);
          gtk_label_set_markup(GTK_LABEL(markup2ascii),argument_name);
          cimg_snprintf(argument_name,argument_name.width(),"%s",gtk_label_get_text(GTK_LABEL(markup2ascii)));
          cimg::strpare(_argument_type,' ',false,true);
          cimg::strpare(argument_arg,' ',false,true);

          const bool is_silent_argument = (*_argument_type=='_');
          char
            *const argument_type = _argument_type.data() + (is_silent_argument?1:0);

          CImg<char>
            argument_fave = get_fave_parameter(filter,current_argument),
            argument_value = get_filter_parameter(filter,current_argument);

          // Check for a float-valued argument.
          bool found_valid_argument = false;
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"float")) {
            float value = 0, min_value = 0, max_value = 100;
            setlocale(LC_NUMERIC,"C");
            cimg_sscanf(argument_arg,"%f%*c%f%*c%f",&value,&min_value,&max_value);
            if (is_fave) cimg_sscanf(argument_fave,"%f",&value);
            if (!reset_params) cimg_sscanf(argument_value,"%f",&value);
            GtkObject *const
              scale = gimp_scale_entry_new(GTK_TABLE(table),0,(int)current_table_line,argument_name,50,6,
                                           (double)value,(double)min_value,(double)max_value,
                                           (double)(max_value - min_value)/100,
                                           (double)(max_value - min_value)/20,
                                           2,true,0,0,0,0);
            event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
            event_infos[2*current_argument + 1] = (void*)0;
            on_float_parameter_changed(GTK_ADJUSTMENT(scale),event_infos + 2*current_argument);
            g_signal_connect(scale,"value_changed",G_CALLBACK(on_float_parameter_changed),
                             event_infos + 2*current_argument);
            if (!is_silent_argument)
              g_signal_connect(scale,"value_changed",G_CALLBACK(_gimp_preview_invalidate),0);
            found_valid_argument = true; ++current_argument;
          }

          // Check for an int-valued argument.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"int")) {
            float value = 0, min_value = 0, max_value = 100;
            setlocale(LC_NUMERIC,"C");
            cimg_sscanf(argument_arg,"%f%*c%f%*c%f",&value,&min_value,&max_value);
            if (is_fave) cimg_sscanf(argument_fave,"%f",&value);
            if (!reset_params) cimg_sscanf(argument_value,"%f",&value);
            GtkObject *const
              scale = gimp_scale_entry_new(GTK_TABLE(table),0,(int)current_table_line,argument_name,50,6,
                                           (double)(int)cimg::round(value,1.0f),
                                           (double)(int)cimg::round(min_value,1.0f),
                                           (double)(int)cimg::round(max_value,1.0f),
                                           (double)1,
                                           (double)std::max(1.0f,cimg::round((max_value - min_value)/20,1,1)),
                                           0,true,0,0,0,0);
            event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
            event_infos[2*current_argument + 1] = (void*)0;
            on_int_parameter_changed(GTK_ADJUSTMENT(scale),event_infos + 2*current_argument);
            g_signal_connect(scale,"value_changed",G_CALLBACK(on_int_parameter_changed),
                             event_infos + 2*current_argument);
            if (!is_silent_argument)
              g_signal_connect(scale,"value_changed",G_CALLBACK(_gimp_preview_invalidate),0);
            found_valid_argument = true; ++current_argument;
          }

          // Check for a bool-valued argument.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"bool")) {
            cimg::strpare(argument_arg,' ',false,true);
            cimg::strpare(argument_arg,'\"',true,false);
            bool
              value = !(!*argument_arg || !cimg::strcasecmp(argument_arg,"false") ||
                        (argument_arg[0]=='0' && argument_arg[1]==0));
            if (is_fave && *argument_fave)
              value = !(!cimg::strcasecmp(argument_fave,"false") ||
                        (argument_fave[0]=='0' && argument_fave[1]==0));
            if (!reset_params && *argument_value)
              value = !(!cimg::strcasecmp(argument_value,"false") ||
                        (argument_value[0]=='0' && argument_value[1]==0));
            GtkWidget *const checkbutton = gtk_check_button_new_with_label(argument_name);
            gtk_widget_show(checkbutton);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),value);
            gtk_table_attach(GTK_TABLE(table),checkbutton,0,3,(int)current_table_line,(int)current_table_line + 1,
                             (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),GTK_SHRINK,0,0);
            event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
            event_infos[2*current_argument + 1] = (void*)0;
            on_bool_parameter_changed(GTK_CHECK_BUTTON(checkbutton),event_infos + 2*current_argument);
            g_signal_connect(checkbutton,"toggled",G_CALLBACK(on_bool_parameter_changed),
                             event_infos + 2*current_argument);
            if (!is_silent_argument)
              g_signal_connect(checkbutton,"toggled",G_CALLBACK(_gimp_preview_invalidate),0);
            found_valid_argument = true; ++current_argument;
          }

          // Check for a button argument.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"button")) {
            float alignment = 0;
            setlocale(LC_NUMERIC,"C");
            if (cimg_sscanf(argument_arg,"%f",&alignment)!=1) alignment = 0;
            GtkWidget
              *const button = gtk_button_new_with_label(argument_name),
              *const align = gtk_alignment_new(alignment,0.5f,0,0);
            gtk_widget_show(button);
            gtk_container_add(GTK_CONTAINER(align),button);
            gtk_widget_show(align);
            gtk_table_attach(GTK_TABLE(table),align,0,3,(int)current_table_line,(int)current_table_line + 1,
                             (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),GTK_SHRINK,0,0);
            event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
            event_infos[2*current_argument + 1] = (void*)0;
            g_signal_connect(button,"clicked",G_CALLBACK(on_button_parameter_clicked),
                             event_infos + 2*current_argument);
            if (!is_silent_argument)
              g_signal_connect(button,"clicked",G_CALLBACK(_gimp_preview_invalidate),0);
            set_filter_parameter(filter,current_argument,"0");
            CImg<unsigned int>::vector(current_argument).move_to(gmic_button_parameters);
            found_valid_argument = true; ++current_argument;
          }

          // Check for a choice-valued argument.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"choice")) {
            GtkWidget *const label = gtk_label_new(argument_name);
            gtk_widget_show(label);
            gtk_table_attach(GTK_TABLE(table),label,0,1,(int)current_table_line,(int)current_table_line + 1,
                             GTK_FILL,GTK_SHRINK,0,0);
            gtk_misc_set_alignment(GTK_MISC(label),0,0.5);
            GtkWidget *const combobox = gtk_combo_box_new_text();
            gtk_widget_show(combobox);
            CImg<char> s_entry(1024); *s_entry = 0;
            char end = 0; int err = 0;
            unsigned int value = 0;
            const char *entries = argument_arg;
            if (cimg_sscanf(entries,"%u",&value)==1)
              entries+=cimg_snprintf(s_entry,s_entry.width(),"%u",value) + 1;
            while (*entries) {
              if ((err = cimg_sscanf(entries,"%1023[^,]%c",s_entry.data(),&end))>0) {
                entries += std::strlen(s_entry) + (err==2?1:0);
                cimg::strpare(s_entry,' ',false,true);
                cimg::strpare(s_entry,'\"',true,false);
                cimg::strunescape(s_entry);
                gtk_label_set_markup(GTK_LABEL(markup2ascii),s_entry);
                cimg_snprintf(s_entry,s_entry.width(),"%s",gtk_label_get_text(GTK_LABEL(markup2ascii)));
                gtk_combo_box_append_text(GTK_COMBO_BOX(combobox),s_entry);
              } else break;
            }
            if (is_fave) cimg_sscanf(argument_fave,"%u",&value);
            if (!reset_params) cimg_sscanf(argument_value,"%u",&value);
            gtk_combo_box_set_active(GTK_COMBO_BOX(combobox),value);
            gtk_table_attach(GTK_TABLE(table),combobox,1,3,(int)current_table_line,(int)current_table_line + 1,
                             (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),GTK_FILL,0,0);
            event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
            event_infos[2*current_argument + 1] = (void*)0;
            on_list_parameter_changed(GTK_COMBO_BOX(combobox),event_infos + 2*current_argument);
            g_signal_connect(combobox,"changed",G_CALLBACK(on_list_parameter_changed),
                             event_infos + 2*current_argument);
            if (!is_silent_argument)
              g_signal_connect(combobox,"changed",G_CALLBACK(_gimp_preview_invalidate),0);
            found_valid_argument = true; ++current_argument;
          }

          // Check for a single or multi-line text-valued argument.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"text")) {
            int line_number = 0;
            char sep = 0;
            if (cimg_sscanf(argument_arg,"%d%c",&line_number,&sep)==2 && sep==',' && line_number==1) {
              // Multi-line entry
              GtkWidget *const frame = gtk_frame_new(NULL);
              gtk_widget_show(frame);
              gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_IN);
              gtk_container_set_border_width(GTK_CONTAINER(frame),4);

              GtkWidget *const hbox = gtk_hbox_new(false,0);
              gtk_widget_show(hbox);

              CImg<char> s_label(256); *s_label = 0;
              cimg_snprintf(s_label,s_label.width(),"  %s       ",argument_name.data());
              GtkWidget *const label = gtk_label_new(s_label);
              gtk_widget_show(label);
              gtk_box_pack_start(GTK_BOX(hbox),label,false,false,0);

              GtkWidget *const button = gtk_button_new_with_label(t("Update"));
              gtk_widget_show(button);
              gtk_box_pack_start(GTK_BOX(hbox),button,false,false,0);

              gtk_frame_set_label_widget(GTK_FRAME(frame),hbox);

              GtkWidget *const alignment2 = gtk_alignment_new(0,0,1,1);
              gtk_widget_show(alignment2);
              gtk_alignment_set_padding(GTK_ALIGNMENT(alignment2),3,3,3,3);
              gtk_container_add(GTK_CONTAINER(frame),alignment2);

              GtkWidget *const view = gtk_text_view_new();
              gtk_widget_show(view);
              gtk_text_view_set_editable(GTK_TEXT_VIEW(view),true);
              gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view),8);
              gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view),8);
              gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view),GTK_WRAP_CHAR);
              gtk_container_add(GTK_CONTAINER(alignment2),view);

              GtkTextBuffer *const buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
              char *value = std::strchr(argument_arg,',') + 1;
              if (is_fave) value = argument_fave;
              if (!reset_params && *argument_value) value = argument_value;
              else if (!is_fave) cimg::strunescape(value);
              cimg::strpare(value,' ',false,true);
              cimg::strpare(value,'\"',true,false);
              gtk_label_set_markup(GTK_LABEL(markup2ascii),value);
              cimg_snprintf(argument_arg,argument_arg.width(),"%s",gtk_label_get_text(GTK_LABEL(markup2ascii)));
              for (char *p = argument_arg; *p; ++p) if (*p==gmic_dquote) *p='\"';
              gtk_text_buffer_set_text(buffer,argument_arg,-1);

              gtk_table_attach(GTK_TABLE(table),frame,0,3,(int)current_table_line,(int)current_table_line + 1,
                               (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),(GtkAttachOptions)0,0,0);

              event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
              event_infos[2*current_argument + 1] = (void*)view;
              on_multitext_parameter_changed(event_infos + 2*current_argument);
              g_signal_connect_swapped(button,"clicked",G_CALLBACK(on_multitext_parameter_changed),
                                       event_infos + 2*current_argument);
              g_signal_connect_swapped(view,"key-release-event",G_CALLBACK(on_multitext_parameter_changed),
                                       event_infos + 2*current_argument);
              if (!is_silent_argument)
                g_signal_connect(button,"clicked",G_CALLBACK(_gimp_preview_invalidate),0);

            } else { // Single-line entry
              GtkWidget *const label = gtk_label_new(argument_name);
              gtk_widget_show(label);
              gtk_table_attach(GTK_TABLE(table),label,0,1,(int)current_table_line,(int)current_table_line + 1,
                               GTK_FILL,GTK_SHRINK,0,0);
              gtk_misc_set_alignment(GTK_MISC(label),0,0.5);
              GtkWidget *const entry = gtk_entry_new_with_max_length(1023);
              gtk_widget_show(entry);

              char *value = (line_number!=0 || sep!=',')?argument_arg:(std::strchr(argument_arg,',') + 1);
              if (is_fave) value = argument_fave;
              if (!reset_params && *argument_value) value = argument_value;
              else if (!is_fave) cimg::strunescape(value);
              cimg::strpare(value,' ',false,true);
              cimg::strpare(value,'\"',true,false);
              gtk_label_set_markup(GTK_LABEL(markup2ascii),value);
              cimg_snprintf(argument_arg,argument_arg.width(),"%s",gtk_label_get_text(GTK_LABEL(markup2ascii)));

              for (char *p = value; *p; ++p) if (*p==gmic_dquote) *p='\"';
              gtk_entry_set_text(GTK_ENTRY(entry),argument_arg);
              gtk_table_attach(GTK_TABLE(table),entry,1,2,(int)current_table_line,(int)current_table_line + 1,
                               (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),(GtkAttachOptions)0,0,0);
              GtkWidget *const button = gtk_button_new_with_label(t("Update"));
              gtk_widget_show(button);
              gtk_table_attach(GTK_TABLE(table),button,2,3,(int)current_table_line,(int)current_table_line + 1,
                               GTK_FILL,GTK_SHRINK,0,0);
              event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
              event_infos[2*current_argument + 1] = (void*)entry;
              on_text_parameter_changed(event_infos + 2*current_argument);
              g_signal_connect_swapped(button,"clicked",G_CALLBACK(on_text_parameter_changed),
                                       event_infos + 2*current_argument);
              g_signal_connect_swapped(entry,"changed",G_CALLBACK(on_text_parameter_changed),
                                       event_infos + 2*current_argument);
              if (!is_silent_argument) {
                g_signal_connect(button,"clicked",G_CALLBACK(_gimp_preview_invalidate),0);
                g_signal_connect(entry,"activate",G_CALLBACK(_gimp_preview_invalidate),0);
              }
            }

            found_valid_argument = true; ++current_argument;
          }

          // Check for a filename or folder name argument.
          if (!found_valid_argument && (!cimg::strcasecmp(argument_type,"file") ||
                                        !cimg::strcasecmp(argument_type,"folder"))) {
            GtkWidget *const label = gtk_label_new(argument_name);
            gtk_widget_show(label);
            gtk_table_attach(GTK_TABLE(table),label,0,1,(int)current_table_line,(int)current_table_line + 1,
                             GTK_FILL,GTK_SHRINK,0,0);
            gtk_misc_set_alignment(GTK_MISC(label),0,0.5);
            GtkWidget *const
              file_chooser = gtk_file_chooser_button_new(argument_name,
                                                         cimg::lowercase(argument_type[1])=='i'?
                                                         GTK_FILE_CHOOSER_ACTION_OPEN:
                                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
            gtk_widget_show(file_chooser);

            char *value = argument_arg;
            if (is_fave) value = argument_fave;
            if (!reset_params && *argument_value) value = argument_value;
            cimg::strpare(value,' ',false,true);
            cimg::strpare(value,'\"',true,false);
            gtk_label_set_markup(GTK_LABEL(markup2ascii),value);
            cimg_snprintf(argument_arg,argument_arg.width(),"%s",gtk_label_get_text(GTK_LABEL(markup2ascii)));
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_chooser),value);
            gtk_table_attach(GTK_TABLE(table),file_chooser,1,3,(int)current_table_line,(int)current_table_line + 1,
                             (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),(GtkAttachOptions)0,0,0);
            event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
            event_infos[2*current_argument + 1] = (void*)is_silent_argument; // Silent property handled by event.
            on_file_parameter_changed(GTK_FILE_CHOOSER(file_chooser),event_infos + 2*current_argument);
            g_signal_connect(file_chooser,"selection-changed",G_CALLBACK(on_file_parameter_changed),
                             event_infos + 2*current_argument);
            found_valid_argument = true; ++current_argument;
          }

          // Check for a color argument.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"color")) {
            GtkWidget *const label = gtk_label_new(argument_name);
            gtk_widget_show(label);
            gtk_misc_set_alignment(GTK_MISC(label),0,0.5);
            gtk_table_attach(GTK_TABLE(table),label,0,1,(int)current_table_line,(int)current_table_line + 1,
                             GTK_FILL,GTK_SHRINK,0,0);
            GtkWidget *const hbox = gtk_hbox_new(false,6);
            gtk_widget_show(hbox);
            gtk_table_attach(GTK_TABLE(table),hbox,1,2,(int)current_table_line,(int)current_table_line + 1,
                             GTK_FILL,GTK_SHRINK,0,0);
            GtkWidget *const color_chooser = gtk_color_button_new();
            gtk_widget_show(color_chooser);
            gtk_color_button_set_title(GTK_COLOR_BUTTON(color_chooser),argument_name);
            gtk_button_set_alignment(GTK_BUTTON(color_chooser),0,0.5);
            gtk_box_pack_start(GTK_BOX(hbox),color_chooser,false,false,0);

            float red = 0, green = 0, blue = 0, alpha = 255;
            setlocale(LC_NUMERIC,"C");

            const int err = cimg_sscanf(argument_arg,"%f%*c%f%*c%f%*c%f",&red,&green,&blue,&alpha);
            if (is_fave) cimg_sscanf(argument_fave,"%f%*c%f%*c%f%*c%f",&red,&green,&blue,&alpha);
            if (!reset_params) cimg_sscanf(argument_value,"%f%*c%f%*c%f%*c%f",&red,&green,&blue,&alpha);
            red = red<0?0:red>255?255:red;
            green = green<0?0:green>255?255:green;
            blue = blue<0?0:blue>255?255:blue;
            GdkColor color;
            color.pixel = 0;
            color.red = (unsigned int)(red*257);
            color.green = (unsigned int)(green*257);
            color.blue = (unsigned int)(blue*257);
            gtk_color_button_set_color(GTK_COLOR_BUTTON(color_chooser),&color);
            if (err==4) {
              gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(color_chooser),true);
              gtk_color_button_set_alpha(GTK_COLOR_BUTTON(color_chooser),(unsigned int)(alpha*257));
            } else gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(color_chooser),false);
            event_infos[2*current_argument] = (void*)(cimg_ulong)current_argument;
            event_infos[2*current_argument + 1] = (void*)0;
            on_color_parameter_changed(GTK_COLOR_BUTTON(color_chooser),event_infos + 2*current_argument);
            g_signal_connect(color_chooser,"color-set",G_CALLBACK(on_color_parameter_changed),
                             event_infos + 2*current_argument);
            if (!is_silent_argument)
              g_signal_connect(color_chooser,"color-set",G_CALLBACK(_gimp_preview_invalidate),0);
            found_valid_argument = true; ++current_argument;
          }

          // Check for a constant value.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"const")) {
            const char *value = argument_arg;
            if (is_fave) value = argument_fave;
            if (!reset_params && *argument_value) value = argument_value;
            set_filter_parameter(filter,current_argument,value);
            found_valid_argument = true; ++current_argument;
          }

          // Check for a note.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"note")) {
            cimg::strpare(argument_arg,' ',false,true);
            cimg::strpare(argument_arg,'\"',true,false);
            cimg::strunescape(argument_arg);

            const char *const tag_s = "<span style=\"color:", *const tag_d = "<span color=\"      ";
            char *ptr = std::strstr(argument_arg,tag_s);  // Search & replace to allow HTML syntax for colored text
            while (ptr) { std::memcpy(ptr,tag_d,std::strlen(tag_d)); ptr = std::strstr(++ptr,tag_s); }

            GtkWidget *const label = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(label),argument_arg);
            gtk_label_set_line_wrap(GTK_LABEL(label),true);
            gtk_widget_show(label);
            gtk_table_attach(GTK_TABLE(table),label,0,3,(int)current_table_line,(int)current_table_line + 1,
                             (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),GTK_SHRINK,0,0);
            gtk_misc_set_alignment(GTK_MISC(label),0,0.5);
            found_valid_argument = true;
          }

          // Check for a link.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"link")) {
            CImg<char> label(1024), url(1024); *label = *url = 0;
            float alignment = 0.5f;
            switch (cimg_sscanf(argument_arg,"%f,%1023[^,],%1023s",&alignment,label.data(),url.data())) {
            case 2 : std::strcpy(url,label); break;
            case 1 : cimg_snprintf(url,url.width(),"%g",alignment); break;
            case 0 : if (cimg_sscanf(argument_arg,"%1023[^,],%1023s",label.data(),url.data())==1)
                std::strcpy(url,label);
              break;
            }
            cimg::strpare(label,' ',false,true);
            cimg::strpare(label,'\"',true,false);
            cimg::strunescape(label);
            gtk_label_set_markup(GTK_LABEL(markup2ascii),label);
            cimg_snprintf(label,label.width(),"%s",gtk_label_get_text(GTK_LABEL(markup2ascii)));
            cimg::strpare(url,' ',false,true);
            cimg::strpare(url,'\"',true,false);
            GtkWidget *const link = gtk_link_button_new_with_label(url,label);
            gtk_widget_show(link);
            gtk_button_set_alignment(GTK_BUTTON(link),alignment,0.5);
            gtk_table_attach(GTK_TABLE(table),link,0,3,(int)current_table_line,(int)current_table_line + 1,
                             (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),GTK_SHRINK,0,0);
            found_valid_argument = true;
          }

          // Check for an horizontal separator.
          if (!found_valid_argument && !cimg::strcasecmp(argument_type,"separator")) {
            GtkWidget *const separator = gtk_hseparator_new();
            gtk_widget_show(separator);
            gtk_table_attach(GTK_TABLE(table),separator,0,3,(int)current_table_line,(int)current_table_line + 1,
                             (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),GTK_SHRINK,0,0);
            found_valid_argument = true;
          }

          if (!found_valid_argument) {
            if (get_verbosity_mode()>1) {
              std::fprintf(cimg::output(),
                           "\n[gmic_gimp]./error/ Found invalid parameter type '%s' for argument '%s'.\n",
                           argument_type,argument_name.data());
              std::fflush(cimg::output());
            }
          } else ++current_table_line;
        } else break;
      }
      set_filter_nbparams(filter,current_argument);
    }

    // Add preview warning message (initially hidden).
    gui_preview_warning = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(gui_preview_warning),
                         t("\n<span color=\"#AA0000\"><b>Warning:</b> Preview may be inaccurate\n"
                           "(zoom factor has been modified)</span>"));
    gtk_table_attach(GTK_TABLE(table),gui_preview_warning,0,3,(int)current_table_line,(int)current_table_line + 1,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),GTK_SHRINK,0,0);
  }

  gtk_container_add(GTK_CONTAINER(right_frame),table);

  // Take care of the size of the parameter table.
  GtkRequisition requisition;
  gtk_widget_size_request(table,&requisition);
  gtk_widget_set_size_request(right_pane,std::max(450,requisition.width),-1);
  set_preview_factor();

  // Set correct icon for fave button.
  if (fave_stock) gtk_widget_destroy(fave_stock);
  fave_stock = gtk_button_new_from_stock(!filter?GTK_STOCK_ABOUT:GTK_STOCK_ADD);
  GtkWidget *const fave_image = gtk_button_get_image(GTK_BUTTON(fave_stock));
  gtk_button_set_image(GTK_BUTTON(fave_add_button),fave_image);
  gtk_widget_show(fave_add_button);
  if (filter && filter>=indice_faves) gtk_widget_show(fave_delete_button);
  else gtk_widget_hide(fave_delete_button);
}

// Create main plug-in dialog window and wait for events.
//-------------------------------------------------------
bool create_dialog_gui() {

  // Init GUI_specific variables
  _create_dialog_gui = true;
  gimp_ui_init("gmic",true);
  event_infos = 0;

  // Create main dialog window with buttons.
  CImg<char> dialog_title(64);
#ifdef gmic_prerelease
  cimg_snprintf(dialog_title,dialog_title.width(),"%s %d.%d - %s %u bits - %d.%d.%dpre#%s",
                t("G'MIC for GIMP"),
                GIMP_MAJOR_VERSION,GIMP_MINOR_VERSION,
                cimg::stros(),sizeof(void*)==8?64:32,
                gmic_version/100,(gmic_version/10)%10,gmic_version%10,gmic_prerelease);
#else
  cimg_snprintf(dialog_title,dialog_title.width(),"%s %d.%d - %s %u bits - %d.%d.%d",
                t("G'MIC for GIMP"),
                GIMP_MAJOR_VERSION,GIMP_MINOR_VERSION,
                cimg::stros(),sizeof(void*)==8?64:32,
                gmic_version/100,(gmic_version/10)%10,gmic_version%10);
#endif

  dialog_window = gimp_dialog_new(dialog_title,"gmic",0,(GtkDialogFlags)0,0,0,NULL);
  gimp_window_set_transient(GTK_WINDOW(dialog_window));

  g_signal_connect(dialog_window,"close",G_CALLBACK(on_dialog_cancel_clicked),0);
  g_signal_connect(dialog_window,"delete-event",G_CALLBACK(on_dialog_cancel_clicked),0);

  GtkWidget *const cancel_button = gtk_dialog_add_button(GTK_DIALOG(dialog_window),
                                                         GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL);
  g_signal_connect(cancel_button,"clicked",G_CALLBACK(on_dialog_cancel_clicked),0);

  GtkWidget *const maximize_button = gtk_dialog_add_button(GTK_DIALOG(dialog_window),
                                                           GTK_STOCK_FULLSCREEN,GTK_RESPONSE_NONE);
  g_signal_connect(maximize_button,"clicked",G_CALLBACK(on_dialog_maximize_button_clicked),0);

  GtkWidget *const reset_button = gtk_dialog_add_button(GTK_DIALOG(dialog_window),
                                                        GIMP_STOCK_RESET,GTK_RESPONSE_NONE);
  g_signal_connect(reset_button,"clicked",G_CALLBACK(on_dialog_reset_clicked),0);

  GtkWidget *apply_button = gtk_dialog_add_button(GTK_DIALOG(dialog_window),
                                                  GTK_STOCK_APPLY,GTK_RESPONSE_APPLY);
  g_signal_connect(apply_button,"clicked",G_CALLBACK(on_dialog_apply_clicked),0);

  GtkWidget *ok_button = gtk_dialog_add_button(GTK_DIALOG(dialog_window),
                                               GTK_STOCK_OK,GTK_RESPONSE_OK);
  g_signal_connect(ok_button,"clicked",G_CALLBACK(gtk_main_quit),0);

  GtkWidget *const dialog_hpaned = gtk_hpaned_new();
  gtk_widget_show(dialog_hpaned);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_window)->vbox),dialog_hpaned);

  // Create the left pane.
  GtkWidget *const left_pane = gtk_vbox_new(false,4);
  gtk_widget_show(left_pane);
  gtk_paned_pack1(GTK_PANED(dialog_hpaned),left_pane,true,false);

  GtkWidget *const left_hbox = gtk_hbox_new(false,4);
  gtk_widget_show(left_hbox);
  gtk_box_pack_end(GTK_BOX(left_pane),left_hbox,false,false,0);

  GtkWidget *const image_align = gtk_alignment_new(0.5,0.5,0,0);
  gtk_widget_show(image_align);
  gtk_box_pack_start(GTK_BOX(left_hbox),image_align,true,true,0);
  CImg<unsigned char> gmic_logo =
    CImgList<unsigned char>::get_unserialize(CImg<unsigned char>(data_gmic_logo,1,size_data_gmic_logo,1,1,true))[0];
  const unsigned int logo_width = gmic_logo._width, logo_height = gmic_logo._height;
  gmic_logo.permute_axes("cxyz");
  GdkPixbuf *const pixbuf = gdk_pixbuf_new_from_data(gmic_logo,GDK_COLORSPACE_RGB,
                                                     true,8,logo_width,logo_height,4*logo_width,0,0);

  GtkWidget *const image = gtk_image_new_from_pixbuf(pixbuf);
  gtk_widget_set_tooltip_text(image,
                              "G'MIC (http://gmic.eu)\n"
                              "GREYC (http://www.greyc.fr)\n"
                              "CNRS (http://www.cnrs.fr)\n"
                              "Normandy University (http://www.normandie-univ.fr)\n"
                              "ENSICAEN (http://www.ensicaen.fr)");
  // GtkWidget *const image_but = gtk_link_button_new_with_label("http://gmic.eu","");
  // gtk_widget_show(image_but);
  // gtk_button_set_image(GTK_BUTTON(image_but),image); // Doesn't display image on Windows...
  gtk_widget_show(image);
  gtk_container_add(GTK_CONTAINER(image_align),image);

  GtkWidget *const right_align = gtk_alignment_new(1,0,0,0);
  gtk_widget_show(right_align);
  gtk_box_pack_start(GTK_BOX(left_hbox),right_align,true,true,0);

  GtkWidget *const left_frame = gtk_frame_new(NULL);
  gtk_widget_show(left_frame);
  gtk_container_set_border_width(GTK_CONTAINER(left_frame),4);
  gtk_container_add(GTK_CONTAINER(right_align),left_frame);

  GtkWidget *const frame_title = gtk_label_new(NULL);
  gtk_widget_show(frame_title);
  gtk_label_set_markup(GTK_LABEL(frame_title),t("<b> Input / Output </b>"));
  gtk_frame_set_label_widget(GTK_FRAME(left_frame),frame_title);

  GtkWidget *const left_table = gtk_table_new(5,1,false);
  gtk_widget_show(left_table);
  gtk_table_set_row_spacings(GTK_TABLE(left_table),6);
  gtk_table_set_col_spacings(GTK_TABLE(left_table),6);
  gtk_container_set_border_width(GTK_CONTAINER(left_table),8);
  gtk_container_add(GTK_CONTAINER(left_frame),left_table);

  GtkWidget *const input_combobox = gtk_combo_box_new_text();
  gtk_widget_show(input_combobox);
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("Input layers..."));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),"-");
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("None"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("Active (default)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("All"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("Active & below"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("Active & above"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("All visibles"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("All invisibles"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("All visibles (decr.)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("All invisibles (decr.)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(input_combobox),t("All (decr.)"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(input_combobox),get_input_mode(false));
  gtk_table_attach_defaults(GTK_TABLE(left_table),input_combobox,0,1,0,1);
  g_signal_connect(input_combobox,"changed",G_CALLBACK(on_dialog_input_mode_changed),0);

  GtkWidget *const output_combobox = gtk_combo_box_new_text();
  gtk_widget_show(output_combobox);
  gtk_combo_box_append_text(GTK_COMBO_BOX(output_combobox),t("Output mode..."));
  gtk_combo_box_append_text(GTK_COMBO_BOX(output_combobox),"-");
  gtk_combo_box_append_text(GTK_COMBO_BOX(output_combobox),t("In place (default)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(output_combobox),t("New layer(s)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(output_combobox),t("New active layer(s)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(output_combobox),t("New image"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(output_combobox),get_output_mode(false));
  gtk_table_attach_defaults(GTK_TABLE(left_table),output_combobox,0,1,1,2);
  g_signal_connect(output_combobox,"changed",G_CALLBACK(on_dialog_output_mode_changed),0);

  GtkWidget *const verbosity_combobox = gtk_combo_box_new_text();
  gtk_widget_show(verbosity_combobox);
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Output messages..."));
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),"-");
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Quiet (default)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Verbose (layer name)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Verbose (console)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Verbose (logfile)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Very verbose (console)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Very verbose (logfile)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Debug mode (console)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(verbosity_combobox),t("Debug mode (logfile)"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(verbosity_combobox),get_verbosity_mode(false));
  gtk_table_attach_defaults(GTK_TABLE(left_table),verbosity_combobox,0,1,2,3);
  g_signal_connect(verbosity_combobox,"changed",G_CALLBACK(on_dialog_verbosity_mode_changed),0);

  GtkWidget *const preview_mode_combobox = gtk_combo_box_new_text();
  gtk_widget_show(preview_mode_combobox);
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("Preview mode..."));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),"-");
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("1st output (default)"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("2nd output"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("3rd output"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("4th output"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("1st -> 2nd"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("1st -> 3rd"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("1st -> 4th"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_mode_combobox),t("All outputs"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(preview_mode_combobox),get_preview_mode(false));
  gtk_table_attach_defaults(GTK_TABLE(left_table),preview_mode_combobox,0,1,3,4);
  g_signal_connect(preview_mode_combobox,"changed",G_CALLBACK(on_dialog_preview_mode_changed),0);

  GtkWidget *preview_size_combobox = gtk_combo_box_new_text();
  gtk_widget_show(preview_size_combobox);
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_size_combobox),t("Preview size..."));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_size_combobox),"-");
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_size_combobox),t("Tiny"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_size_combobox),t("Small"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_size_combobox),t("Normal"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(preview_size_combobox),t("Large"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(preview_size_combobox),get_preview_size(false));
  gtk_table_attach_defaults(GTK_TABLE(left_table),preview_size_combobox,0,1,4,5);
  g_signal_connect(preview_size_combobox,"changed",G_CALLBACK(on_dialog_preview_size_changed),0);

  gui_preview_align = gtk_alignment_new(0.5,0.5,0,0);
  gtk_widget_show(gui_preview_align);
  gtk_box_pack_end(GTK_BOX(left_pane),gui_preview_align,true,true,0);

  gui_preview = 0;
  _gimp_preview_invalidate();
  g_signal_connect(dialog_window,"size-request",G_CALLBACK(on_dialog_resized),0);

  // Create the middle pane.
  GtkWidget *const mr_hpaned = gtk_hpaned_new();
  gtk_widget_show(mr_hpaned);
  gtk_paned_pack2(GTK_PANED(dialog_hpaned),mr_hpaned,true,true);

  GtkWidget *const middle_frame = gtk_frame_new(NULL);
  gtk_widget_show(middle_frame);
  gtk_container_set_border_width(GTK_CONTAINER(middle_frame),4);
  gtk_paned_add1(GTK_PANED(mr_hpaned),middle_frame);

  GtkWidget *const middle_pane = gtk_vbox_new(false,4);
  gtk_widget_show(middle_pane);
  gtk_container_add(GTK_CONTAINER(middle_frame),middle_pane);

  relabel_hbox = gtk_hbox_new(false,3);
  gtk_box_pack_start(GTK_BOX(middle_pane),relabel_hbox,false,false,0);
  relabel_entry = gtk_entry_new_with_max_length(255);
  gtk_widget_show(relabel_entry);
  gtk_box_pack_start(GTK_BOX(relabel_hbox),relabel_entry,false,true,0);
  GtkWidget *const relabel_button = gtk_button_new_with_label(t("Rename"));
  gtk_widget_show(relabel_button);
  gtk_box_pack_start(GTK_BOX(relabel_hbox),relabel_button,false,true,0);
  g_signal_connect(relabel_button,"clicked",G_CALLBACK(_on_filter_doubleclicked),0);
  g_signal_connect(relabel_entry,"activate",G_CALLBACK(_on_filter_doubleclicked),0);

  GtkWidget *const scrolled_window = gtk_scrolled_window_new(NULL,NULL);
  gtk_widget_show(scrolled_window);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(middle_pane),scrolled_window,true,true,0);

  tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_view_store));
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view),true);
  gtk_widget_show(tree_view);
  gtk_container_add(GTK_CONTAINER(scrolled_window),tree_view);

  GtkWidget *const tree_hbox = gtk_hbox_new(false,6);
  gtk_widget_show(tree_hbox);
  gtk_box_pack_start(GTK_BOX(middle_pane),tree_hbox,false,false,0);

  fave_add_button = gtk_button_new();
  gtk_box_pack_start(GTK_BOX(tree_hbox),fave_add_button,false,false,0);
  g_signal_connect_swapped(fave_add_button,"clicked",G_CALLBACK(on_dialog_add_fave_clicked),tree_view);
  fave_delete_button = gtk_button_new();
  gtk_box_pack_start(GTK_BOX(tree_hbox),fave_delete_button,false,false,0);
  g_signal_connect_swapped(fave_delete_button,"clicked",G_CALLBACK(on_dialog_remove_fave_clicked),tree_view);
  delete_stock = gtk_button_new_from_stock(GTK_STOCK_DELETE);
  GtkWidget *const delete_image = gtk_button_get_image(GTK_BUTTON(delete_stock));
  gtk_button_set_image(GTK_BUTTON(fave_delete_button),delete_image);

  GtkWidget *const refresh_button = gtk_button_new();
  refresh_stock = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
  GtkWidget *const refresh_image = gtk_button_get_image(GTK_BUTTON(refresh_stock));
  gtk_button_set_image(GTK_BUTTON(refresh_button),refresh_image);
  gtk_widget_show(refresh_button);
  gtk_box_pack_start(GTK_BOX(tree_hbox),refresh_button,false,false,0);
  g_signal_connect_swapped(refresh_button,"clicked",G_CALLBACK(on_dialog_refresh_clicked),tree_view);

  GtkWidget *const internet_checkbutton = gtk_check_button_new_with_mnemonic(t("Internet"));
  gtk_widget_show(internet_checkbutton);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(internet_checkbutton),get_net_update());
  gtk_box_pack_start(GTK_BOX(tree_hbox),internet_checkbutton,false,false,0);
  g_signal_connect(internet_checkbutton,"toggled",G_CALLBACK(on_dialog_net_update_toggled),0);

  tree_mode_button = gtk_button_new();
  gtk_box_pack_start(GTK_BOX(tree_hbox),tree_mode_button,false,false,0);
  g_signal_connect_swapped(tree_mode_button,"clicked",G_CALLBACK(on_dialog_tree_mode_clicked),tree_view);

  GtkTreeViewColumn *const column = gtk_tree_view_column_new();
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),column);
  flush_tree_view(tree_view);
  GtkRequisition requisition;
  gtk_widget_size_request((GtkWidget*)tree_view,&requisition);
  gtk_widget_set_size_request((GtkWidget*)tree_view,std::max(210,requisition.width),-1);
  g_signal_connect(tree_view,"cursor-changed",G_CALLBACK(on_filter_selected),0);
  g_signal_connect(tree_view,"row-activated",G_CALLBACK(on_filter_doubleclicked),0);

  // Create the right pane.
  right_pane = gtk_scrolled_window_new(NULL,NULL);
  gtk_widget_show(right_pane);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(right_pane),
                                 GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_paned_add2(GTK_PANED(mr_hpaned),right_pane);

  right_frame = gtk_frame_new(NULL);
  gtk_widget_show(right_frame);
  gtk_container_set_border_width(GTK_CONTAINER(right_frame),4);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(right_pane),right_frame);

  // Define tooltips.
  gtk_widget_set_tooltip_text(fave_add_button,t("Add filter to faves"));
  gtk_widget_set_tooltip_text(fave_delete_button,t("Remove filter from faves"));
  gtk_widget_set_tooltip_text(refresh_button,t("Update filters"));
  gtk_widget_set_tooltip_text(internet_checkbutton,t("Enable Internet updates"));
  gtk_widget_set_tooltip_text(tree_mode_button,t("Expand/collapse"));

  // Finalize dialog window.
  create_parameters_gui(false);
  gtk_widget_grab_focus(tree_view);

  // Make sure the dialog window is entirely visible on screen.
  unsigned int preview_size = get_preview_size(false);
  for (bool is_entirely_visible = false; !is_entirely_visible && preview_size>2; ) {
    GdkScreen *const screen = gtk_window_get_screen(GTK_WINDOW(dialog_window));
    int
      window_width, window_height,
      screen_width = gdk_screen_get_width(screen),
      screen_height = gdk_screen_get_height(screen);
    gtk_window_get_size(GTK_WINDOW(dialog_window),&window_width,&window_height);
    if (window_width>screen_width || window_height>screen_height) resize_preview(--preview_size - 2);
    else { resize_preview(preview_size - 2); is_entirely_visible = true; }
  }
  gtk_widget_show(dialog_window);
  gtk_main();

  // Destroy dialog box widget and free resources.
  gtk_widget_destroy(dialog_window);
  if (tree_mode_stock) gtk_widget_destroy(tree_mode_stock);
  if (fave_stock) gtk_widget_destroy(fave_stock);
  if (delete_stock) gtk_widget_destroy(delete_stock);
  if (refresh_stock) gtk_widget_destroy(refresh_stock);
  if (event_infos) delete[] event_infos;
  return _create_dialog_gui;
}

// 'Run' function, required by the GIMP plug-in API.
//--------------------------------------------------
void gmic_run(const gchar *name, gint nparams, const GimpParam *param,
              gint *nreturn_vals, GimpParam **return_vals) {

  // Init plug-in variables.
#if GIMP_MINOR_VERSION>8
  gegl_init(NULL,NULL);
  gimp_plugin_enable_precision();
#endif
  markup2ascii = gtk_label_new(0);

  static GimpParam return_values[1];
  *return_vals  = return_values;
  *nreturn_vals = 1;
  return_values[0].type = GIMP_PDB_STATUS;
  cimg::unused(name,nparams);
  run_mode = (GimpRunMode)param[0].data.d_int32;
  set_logfile();
  set_locale();
  status = GIMP_PDB_SUCCESS;
#if cimg_OS==2
  cimg::curl_path("_gmic\\curl",true);
#endif
  unsigned int preview_size = get_preview_size(false);
  if (!preview_size) { // Try to guess best preview size.
    unsigned int h = 0;
    try {
      const unsigned int cimg_exception_mode = cimg::exception_mode();
      cimg::exception_mode(0);
      h = CImgDisplay::screen_height();
      cimg::exception_mode(cimg_exception_mode);
    } catch (...) { h = 800; }
    preview_size = h>=1024?4:h>=800?3:2;
    set_preview_size(preview_size);
  }

  // Create resources directory.
  if (!gmic::init_rc()) {
    std::fprintf(cimg::output(),
                 "\n[gmic_gimp]./error/ Unable to create resources directory 'gmicrc'.\n");
    std::fflush(cimg::output());
  }

  // Update filters.
  update_filters(false,true);

  try {
    image_id = param[1].data.d_drawable;
#if GIMP_MINOR_VERSION<=8
    gimp_tile_cache_ntiles(2*(gimp_image_width(image_id)/gimp_tile_width() + 1));
#endif

    // Check for run mode.
    switch (run_mode) {

    case GIMP_RUN_INTERACTIVE : {

      // Try updating filters automatically if necessary (every week).
      CImg<char> str(std::strlen(gmic::path_rc()) + 32);
      cimg_snprintf(str,str.width(),"%sgimp_update.lock",
                    gmic::path_rc());
      bool try_network_update = true;
      gimp_get_data("gmic_lock_update",&try_network_update);
      if (try_network_update) {
        const int date = cimg::date(2), fdate = cimg::fdate(str,2);
        if (fdate>=0 && cimg::mod(date - fdate,31)<7) try_network_update = false;
      }
      if (try_network_update) {
        update_filters(true,true);
        std::FILE *lockfile = std::fopen(str,"wb");
        if (lockfile) std::fclose(lockfile);
        try_network_update = false;
        gimp_set_data("gmic_lock_update",&try_network_update,sizeof(bool));
      }

      // Display dialog window.
      if (create_dialog_gui()) {
        cimg::mutex(25);
        if (p_spt) { st_process_thread &spt = *(st_process_thread*)p_spt; spt.is_abort = true; }
        cimg::mutex(25,0);
        process_image(0,false);
        const CImg<char> command_line = get_command_line(false);
        if (command_line) { // Remember command line for the next use of the filter.
          CImg<char> s_tmp(48);
          cimg_snprintf(s_tmp,s_tmp.width(),"gmic_command_line%u",get_current_filter());
          gimp_set_data(s_tmp,command_line,std::strlen(command_line) + 1);
        }
      }
    } break;

    case GIMP_RUN_WITH_LAST_VALS : {
      const unsigned int filter = get_current_filter();
      if (filter) {
        CImg<char> s_tmp(48);
        cimg_snprintf(s_tmp,s_tmp.width(),"gmic_command_line%u",filter);
        const unsigned int siz = 1U + gimp_get_data_size(s_tmp);
        CImg<char> command_line(2*siz); *command_line = 0;
        gimp_get_data(s_tmp,command_line);
        process_image(command_line,false);
        const CImg<char> next_command_line = get_command_line(false);
        // Remember command line for the next use of the filter.
        if (next_command_line) gimp_set_data(s_tmp,next_command_line,std::strlen(next_command_line) + 1);
      }
    } break;

    case GIMP_RUN_NONINTERACTIVE : {
      const unsigned int _input_mode = get_input_mode(), _output_mode = get_output_mode();
      set_input_mode(param[3].data.d_int32 + 2);
      set_output_mode(0);
      process_image(param[4].data.d_string,false);
      set_input_mode(_input_mode + 2);
      set_output_mode(_output_mode + 2);
    } break;
    }

  } catch (CImgException &e) {
    GtkWidget *const
      message = gtk_message_dialog_new(0,GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,"%s",e.what());
    gtk_widget_show(message);
    gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);
    status = GIMP_PDB_CALLING_ERROR;
  }

  gtk_widget_destroy(markup2ascii);
  if (preview_image_id) gimp_image_delete(preview_image_id);
  if (logfile) std::fclose(logfile);
  return_values[0].data.d_status = status;
}

// 'Query' function, required by the GIMP plug-in API.
//----------------------------------------------------
void gmic_query() {
  static const GimpParamDef args[] = {
    {GIMP_PDB_INT32,    (gchar*)"run_mode", (gchar*)"Interactive, non-interactive"},
    {GIMP_PDB_IMAGE,    (gchar*)"image",    (gchar*)"Input image"},
    {GIMP_PDB_DRAWABLE, (gchar*)"drawable", (gchar*)"Input drawable (unused)"},
    {GIMP_PDB_INT32,    (gchar*)"input",    (gchar*)"Input layers mode, when non-interactive"
     "(0=none, 1=active, 2=all, 3=active & below, 4=active & above, 5=all visibles, 6=all invisibles, "
     "7=all visibles (decr.), 8=all invisibles (decr.), 9=all (decr.))"},
    {GIMP_PDB_STRING,   (gchar*)"command",  (gchar*)"G'MIC command string, when non-interactive"},
  };

  set_locale();
  gimp_install_procedure("plug-in-gmic",             // name
                         "G'MIC",                    // blurb
                         "G'MIC",                    // help
                         "David Tschumperl\303\251", // author
                         "David Tschumperl\303\251", // copyright
                         "2015",                     // date
                         "_G'MIC...",                // menu_path
                         "RGB*, GRAY*",              // image_types
                         GIMP_PLUGIN,                // type
                         G_N_ELEMENTS(args),         // nparams
                         0,                          // nreturn_vals
                         args,                       // params
                         0);                         // return_vals

  gimp_plugin_menu_register("plug-in-gmic", "<Image>/Filters");
}

GimpPlugInInfo PLUG_IN_INFO = { 0, 0, gmic_query, gmic_run };
MAIN()
