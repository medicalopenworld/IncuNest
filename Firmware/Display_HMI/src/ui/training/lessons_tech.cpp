// Curso Tecnico (spec hmi-training-courses). T0 intro (compartida), T1
// informacion y versiones, T2 WiFi y servidor, T3 idioma y modos, T4 hora, T5
// alarmas tecnicas, T6 actualizacion por el servidor web local, T7 informe de
// soporte, T8 apagado seguro.
#include "ui/training/lessons.h"

#include "UITask.h"
#include "main.h"
#include "ui.h"
#include "ui/HelpDialog.h"
#include "ui/TimeDialog.h"

extern ui_lang_t g_lang;

namespace {

UiControlSnapshot snap() {
  UiControlSnapshot s;
  UI_GetControlSnapshot(&s);
  return s;
}
bool visible(lv_obj_t *o) { return o && lv_obj_is_visible(o); }

bool goalSettingsScreen() { return lv_scr_act() == ui_ScreenSettings; }
bool goalMainScreen() { return lv_scr_act() == ui_ScreenMain; }
bool goalInfoVisible() { return visible(ui_InfoDetailsCont); }
bool goalWifiVisible() {
  return visible(ui_WifiConfigCont) || visible(ui_WifiConnectedCont);
}
bool goalLangVisible() { return visible(ui_LanguagesDropDown); }
bool goalModesVisible() { return visible(ui_ModesConfigCont); }

ui_lang_t s_langBase = LANG_EN;
void enterLangBase() { s_langBase = g_lang; }
bool goalLangChanged() { return g_lang != s_langBase; }

bool s_darkBase = false;
void enterDarkBase() { s_darkBase = snap().darkMode; }
bool goalDarkChanged() { return snap().darkMode != s_darkBase; }

bool goalTimeOpen() { return TimeDialog_IsOpen(); }
bool goalTimeClosed() { return !TimeDialog_IsOpen(); }
bool goalHelpOpen() { return HelpDialog_IsOpen(); }
bool goalHelpClosed() { return !HelpDialog_IsOpen(); }

// ---- T1: informacion y versiones -------------------------------------------------

const Quiz QUIZ_T1 = {
    {T3("El numero de serie", "The serial number", "Le numero de serie"),
     T3("La consigna de temperatura", "The temperature setpoint",
        "La consigne de temperature"),
     T3("El nombre del hospital", "The hospital name", "Le nom de l'hopital")},
    0,
    T3("Soporte cruza el numero de serie con el inventario y con el servidor "
       "de monitorizacion. Va en el asunto del correo de contacto y en el "
       "informe.",
       "Support matches the serial number against the inventory and the "
       "monitoring server. It goes in the contact email subject and in the "
       "report.",
       "Le support croise le numero de serie avec l'inventaire et le serveur "
       "de suivi. Il figure dans l'objet de l'e-mail de contact et le "
       "rapport."),
};

const Step T1_STEPS[] = {
    DO(&ui_Settings, &ui_ScreenMain, goalSettingsScreen,
       "Toca Ajustes.", "Touch Settings.", "Touchez Reglages."),
    DO(&ui_InfoButton, &ui_ScreenSettings, goalInfoVisible,
       "Toca la fila Informacion.", "Touch the Information row.",
       "Touchez la ligne Informations."),
    EXPLAIN(&ui_SNValue, &ui_ScreenSettings,
            "Numero de serie del equipo: identifica esta incubadora en el "
            "inventario y en el servidor. Es lo primero que pide soporte.",
            "Device serial number: identifies this incubator in the inventory "
            "and on the server. It is the first thing support asks for.",
            "Numero de serie de l'appareil : identifie cet incubateur dans "
            "l'inventaire et sur le serveur. Le support le demande en "
            "premier."),
    EXPLAIN(&ui_HMIVerValue, &ui_ScreenSettings,
            "Version del firmware de la pantalla, con fecha de compilacion. Se "
            "actualiza por el servidor web local (leccion 6) o por el "
            "servidor de monitorizacion.",
            "Display firmware version, with build date. It is updated through "
            "the local web server (lesson 6) or the monitoring server.",
            "Version du firmware de l'ecran, avec date de compilation. Mise a "
            "jour via le serveur web local (lecon 6) ou le serveur de suivi."),
    EXPLAIN(&ui_MBVerValue, &ui_ScreenSettings,
            "Version del firmware de la placa de control. Pantalla y placa "
            "son dos equipos distintos con dos firmwares: da los dos a "
            "soporte.",
            "Control board firmware version. Display and board are two "
            "devices with two firmwares: give both to support.",
            "Version du firmware de la carte de controle. Ecran et carte sont "
            "deux appareils avec deux firmwares : donnez les deux au support."),
    DO(&ui_ImgButton2, &ui_ScreenSettings, goalMainScreen,
       "Vuelve a la pantalla principal.", "Go back to the main screen.",
       "Revenez a l'ecran principal."),
    QUIZ(&ui_ScreenMain, &QUIZ_T1,
         "Pregunta: que dato identifica el equipo ante soporte?",
         "Question: which piece of data identifies the device to support?",
         "Question : quelle donnee identifie l'appareil aupres du support ?"),
};

// ---- T2: WiFi y servidor -----------------------------------------------------------

const Quiz QUIZ_T2 = {
    {T3("Sin WiFi la incubadora no regula la temperatura",
        "Without WiFi the incubator does not regulate temperature",
        "Sans WiFi l'incubateur ne regule pas la temperature"),
     T3("Sin WiFi no hay monitorizacion remota ni actualizaciones, pero el "
        "equipo funciona igual",
        "Without WiFi there is no remote monitoring or updates, but the "
        "device works the same",
        "Sans WiFi pas de suivi a distance ni de mises a jour, mais "
        "l'appareil fonctionne pareil"),
     T3("Sin WiFi no suenan las alarmas", "Without WiFi alarms do not sound",
        "Sans WiFi les alarmes ne sonnent pas")},
    1,
    T3("La red solo aporta monitorizacion remota, hora automatica y "
       "actualizaciones. Regulacion y alarmas son locales y no dependen de "
       "ella.",
       "The network only adds remote monitoring, automatic time and updates. "
       "Regulation and alarms are local and do not depend on it.",
       "Le reseau n'apporte que suivi a distance, heure automatique et mises "
       "a jour. Regulation et alarmes sont locales et n'en dependent pas."),
};

const Step T2_STEPS[] = {
    DO(&ui_Settings, &ui_ScreenMain, goalSettingsScreen,
       "Toca Ajustes.", "Touch Settings.", "Touchez Reglages."),
    DO(&ui_WifiButton, &ui_ScreenSettings, goalWifiVisible,
       "Toca la fila WiFi.", "Touch the WiFi row.", "Touchez la ligne WiFi."),
    EXPLAIN(nullptr, &ui_ScreenSettings,
            "Aqui se escriben la red y la contrasena y se pulsa CONECTAR. Si ya "
            "hay conexion, se ve la red actual y DESCONECTAR. En formacion "
            "estos botones estan desactivados.",
            "Here you type the network and password and press CONNECT. If "
            "already connected, you see the current network and DISCONNECT. "
            "In training these buttons are disabled.",
            "Ici vous saisissez le reseau et le mot de passe et CONNECTER. Si "
            "deja connecte, vous voyez le reseau actuel et DECONNECTER. En "
            "formation ces boutons sont desactives."),
    EXPLAIN(&ui_ConnCont, &ui_ScreenSettings,
            "El indicador del heading muestra WIFI o 2G y las barras de "
            "cobertura con el servidor de monitorizacion. La placa tiene "
            "ademas modem 2G propio como respaldo.",
            "The heading indicator shows WIFI or 2G and the signal bars to the "
            "monitoring server. The board also has its own 2G modem as a "
            "fallback.",
            "L'indicateur de l'en-tete montre WIFI ou 2G et les barres de "
            "couverture vers le serveur de suivi. La carte a aussi son modem "
            "2G en secours."),
    DO(&ui_ImgButton2, &ui_ScreenSettings, goalMainScreen,
       "Vuelve a la pantalla principal.", "Go back to the main screen.",
       "Revenez a l'ecran principal."),
    QUIZ(&ui_ScreenMain, &QUIZ_T2,
         "Pregunta: que pasa si la incubadora no tiene WiFi?",
         "Question: what happens if the incubator has no WiFi?",
         "Question : que se passe-t-il si l'incubateur n'a pas de WiFi ?"),
};

// ---- T3: idioma y modos ------------------------------------------------------------

const Quiz QUIZ_T3 = {
    {T3("Solo cambia la pantalla", "Only the screen changes",
        "Seul l'ecran change"),
     T3("Cambia la pantalla y los textos de alarma que manda la placa",
        "The screen and the alarm texts sent by the board change",
        "L'ecran et les textes d'alarme envoyes par la carte changent"),
     T3("Hace falta reiniciar", "A restart is required",
        "Un redemarrage est necessaire")},
    1,
    T3("El idioma se aplica al instante en la pantalla y se envia a la placa, "
       "que traduce los textos de las alarmas. Sin reiniciar.",
       "The language applies at once on the screen and is sent to the board, "
       "which translates the alarm texts. No restart.",
       "La langue s'applique aussitot a l'ecran et est envoyee a la carte, "
       "qui traduit les textes d'alarme. Sans redemarrage."),
};

const Step T3_STEPS[] = {
    DO(&ui_Settings, &ui_ScreenMain, goalSettingsScreen,
       "Toca Ajustes.", "Touch Settings.", "Touchez Reglages."),
    DO(&ui_LanguagesButton, &ui_ScreenSettings, goalLangVisible,
       "Toca la fila Idioma.", "Touch the Language row.",
       "Touchez la ligne Langue."),
    FREE_ENTER(&ui_ScreenSettings, goalLangChanged, enterLangBase,
               "Elige otro idioma en el desplegable. Se aplica al instante. Al "
               "salir de la leccion se restaura el idioma original.",
               "Pick another language in the dropdown. It applies at once. The "
               "original language is restored when the lesson ends.",
               "Choisissez une autre langue dans la liste. Elle s'applique "
               "aussitot. La langue d'origine est restauree a la fin de la "
               "lecon."),
    DO(&ui_ModesButton, &ui_ScreenSettings, goalModesVisible,
       "Toca la fila Modos.", "Touch the Modes row.", "Touchez la ligne Modes."),
    DO_ENTER(&ui_SwitchDarkMode, &ui_ScreenSettings, goalDarkChanged,
             enterDarkBase,
             "Cambia el modo oscuro. Reduce la luz de la pantalla por la "
             "noche; al salir de la leccion vuelve a como estaba.",
             "Toggle dark mode. It dims the screen at night; it goes back to "
             "how it was when the lesson ends.",
             "Basculez le mode sombre. Il reduit la lumiere de l'ecran la "
             "nuit ; il revient a son etat a la fin de la lecon."),
    EXPLAIN(&ui_ModesConfigCont, &ui_ScreenSettings,
            "Modo piel: habilita el control por sonda (exige sonda). Control de "
            "humedad: si el equipo no lo usa, ocultalo y desaparece de la "
            "pantalla principal.",
            "Skin mode: enables probe control (requires the probe). Humidity "
            "control: if the device does not use it, hide it and it "
            "disappears from the main screen.",
            "Mode peau : active le controle par sonde (sonde requise). "
            "Controle d'humidite : si l'appareil ne l'utilise pas, masquez-le "
            "et il disparait de l'ecran principal."),
    DO(&ui_ImgButton2, &ui_ScreenSettings, goalMainScreen,
       "Vuelve a la pantalla principal.", "Go back to the main screen.",
       "Revenez a l'ecran principal."),
    QUIZ(&ui_ScreenMain, &QUIZ_T3,
         "Pregunta: que pasa al cambiar el idioma?",
         "Question: what happens when you change the language?",
         "Question : que se passe-t-il quand on change la langue ?"),
};

// ---- T4: ajustar la hora -------------------------------------------------------------

const Quiz QUIZ_T4 = {
    {T3("La pantalla tiene su propio reloj con pila",
        "The display has its own battery-backed clock",
        "L'ecran a sa propre horloge sur pile"),
     T3("La hora la lleva la placa de control; la pantalla la muestra y la "
        "puede ajustar",
        "The control board keeps the time; the display shows it and can set "
        "it",
        "La carte de controle tient l'heure ; l'ecran l'affiche et peut la "
        "regler"),
     T3("La hora solo llega por WiFi", "Time only arrives over WiFi",
        "L'heure n'arrive que par WiFi")},
    1,
    T3("La placa es la duena del reloj: lo sincroniza por red movil o por "
       "internet cuando puede, y acepta el ajuste manual desde la pantalla.",
       "The board owns the clock: it syncs it over the mobile network or the "
       "internet when it can, and accepts manual setting from the display.",
       "La carte possede l'horloge : elle la synchronise par reseau mobile ou "
       "internet quand elle peut, et accepte le reglage manuel depuis "
       "l'ecran."),
};

const Step T4_STEPS[] = {
    DO(&ui_ClockButton, &ui_ScreenMain, goalTimeOpen,
       "Toca la hora del heading.", "Touch the time in the heading.",
       "Touchez l'heure de l'en-tete."),
    FREE(&ui_ScreenMain, goalTimeClosed,
         "Teclea dia, mes, ano, hora y minutos (10 cifras) y pulsa APLICAR. "
         "En formacion la placa no cambia la hora. Cierra el dialogo para "
         "continuar.",
         "Type day, month, year, hour and minutes (10 digits) and press "
         "APPLY. In training the board does not change the time. Close the "
         "dialog to continue.",
         "Saisissez jour, mois, annee, heure et minutes (10 chiffres) et "
         "APPLIQUER. En formation la carte ne change pas l'heure. Fermez la "
         "boite pour continuer."),
    EXPLAIN(&ui_ClockButton, &ui_ScreenMain,
            "La fecha del heading se pone gris si la hora no esta sincronizada. "
            "Con red movil o internet la placa la corrige sola, zona horaria "
            "incluida.",
            "The heading date turns grey when time is not synced. With mobile "
            "network or internet the board fixes it by itself, time zone "
            "included.",
            "La date de l'en-tete devient grise si l'heure n'est pas "
            "synchronisee. Avec reseau mobile ou internet la carte la corrige "
            "seule, fuseau compris."),
    QUIZ(&ui_ScreenMain, &QUIZ_T4,
         "Pregunta: quien lleva el reloj del equipo?",
         "Question: who keeps the device clock?",
         "Question : qui tient l'horloge de l'appareil ?"),
};

// ---- T5: alarmas tecnicas y que revisar ------------------------------------------------

const Quiz QUIZ_T5 = {
    {T3("Reiniciar la pantalla", "Restart the display", "Redemarrer l'ecran"),
     T3("Revisar el cable y los conectores entre pantalla y placa, y que la "
        "placa este encendida",
        "Check the cable and connectors between display and board, and that "
        "the board is powered",
        "Verifier le cable et les connecteurs entre ecran et carte, et que "
        "la carte est alimentee"),
     T3("Cambiar el idioma", "Change the language", "Changer la langue")},
    1,
    T3("SIN ENLACE CON LA PLACA significa que la pantalla no recibe datos de "
       "la placa de control. Casi siempre es cable, conector o placa "
       "apagada.",
       "NO LINK WITH THE BOARD means the display receives no data from the "
       "control board. It is almost always a cable, a connector or a powered-"
       "off board.",
       "PAS DE LIAISON AVEC LA CARTE signifie que l'ecran ne recoit pas de "
       "donnees de la carte de controle. Presque toujours cable, connecteur "
       "ou carte eteinte."),
};

const Step T5_STEPS[] = {
    EXPLAIN(&ui_AlarmButton, &ui_ScreenMain,
            "Ademas de las clinicas, el equipo tiene alarmas tecnicas. Cada una "
            "trae en el centro de alarmas la accion recomendada. Repasamos las "
            "principales.",
            "Besides the clinical ones, the device has technical alarms. Each "
            "shows its recommended action in the alarm center. Let's review "
            "the main ones.",
            "Outre les alarmes cliniques, l'appareil a des alarmes techniques. "
            "Chacune montre son action recommandee dans le centre d'alarmes. "
            "Revoyons les principales."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Fallo de sensor de aire o de piel: la medida no es fiable y el "
            "control se detiene. Revisa el conector del sensor y sustituyelo "
            "si persiste.",
            "Air or skin sensor fault: the reading is not reliable and control "
            "stops. Check the sensor connector and replace it if it persists.",
            "Defaut de capteur d'air ou de peau : la mesure n'est pas fiable "
            "et le controle s'arrete. Verifiez le connecteur du capteur et "
            "remplacez-le si cela persiste."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Fallo de ventilador o salida de aire obstruida: sin circulacion "
            "el aire se calienta de forma desigual. Revisa el filtro, la "
            "rejilla y que el ventilador gire.",
            "Fan failure or blocked air outlet: without circulation the air "
            "heats unevenly. Check the filter, the grille and that the fan "
            "spins.",
            "Panne de ventilateur ou sortie d'air obstruee : sans circulation "
            "l'air chauffe de facon inegale. Verifiez le filtre, la grille et "
            "la rotation du ventilateur."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Fallo de calefactor o de su sensor: el equipo corta la "
            "calefaccion. Es una averia: contacta con soporte con el numero "
            "de serie.",
            "Heater or heater sensor fault: the device cuts heating. It is a "
            "breakdown: contact support with the serial number.",
            "Defaut de chauffage ou de son capteur : l'appareil coupe le "
            "chauffage. C'est une panne : contactez le support avec le numero "
            "de serie."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Corte de red o bajo voltaje: comprueba el enchufe, el cable y el "
            "generador o SAI. El equipo avisa aunque siga funcionando con "
            "bateria.",
            "Mains interruption or undervoltage: check the plug, the cable and "
            "the generator or UPS. The device warns even while running on "
            "battery.",
            "Coupure secteur ou sous-tension : verifiez la prise, le cable et "
            "le groupe ou l'onduleur. L'appareil avertit meme sur batterie."),
    EXPLAIN(&ui_ConnCont, &ui_ScreenMain,
            "SIN ENLACE CON LA PLACA: la pantalla no recibe datos de la placa "
            "de control y lo indica aqui tachado. Revisa cable y conectores "
            "entre las dos.",
            "NO LINK WITH THE BOARD: the display receives no data from the "
            "control board and shows it crossed out here. Check cable and "
            "connectors between the two.",
            "PAS DE LIAISON AVEC LA CARTE : l'ecran ne recoit rien de la carte "
            "de controle et l'indique ici barre. Verifiez cable et connecteurs "
            "entre les deux."),
    QUIZ(&ui_ScreenMain, &QUIZ_T5,
         "Pregunta: que hay que revisar ante SIN ENLACE CON LA PLACA?",
         "Question: what should you check on NO LINK WITH THE BOARD?",
         "Question : que verifier en cas de PAS DE LIAISON AVEC LA CARTE ?"),
};

// ---- T6: actualizacion por el servidor web local -------------------------------------

const Quiz QUIZ_T6 = {
    {T3("Desde el USB de la pantalla, solo con cable",
        "From the display USB, cable only",
        "Depuis l'USB de l'ecran, cable uniquement"),
     T3("Desde un navegador en la misma red WiFi, con usuario y contrasena",
        "From a browser on the same WiFi network, with user and password",
        "Depuis un navigateur sur le meme reseau WiFi, avec identifiant et "
        "mot de passe"),
     T3("No se puede actualizar", "It cannot be updated",
        "Elle ne peut pas etre mise a jour")},
    1,
    T3("La pantalla tiene un servidor web local: con el equipo en la WiFi, se "
       "entra desde un navegador con usuario y contrasena y se sube el "
       "firmware. Tambien llega por el servidor de monitorizacion.",
       "The display has a local web server: with the device on WiFi, open it "
       "from a browser with user and password and upload the firmware. It "
       "also arrives via the monitoring server.",
       "L'ecran a un serveur web local : appareil en WiFi, ouvrez-le depuis "
       "un navigateur avec identifiant et mot de passe et envoyez le "
       "firmware. Il arrive aussi via le serveur de suivi."),
};

const Step T6_STEPS[] = {
    EXPLAIN(&ui_Settings, &ui_ScreenMain,
            "Actualizar el firmware de la pantalla: 1) conecta el equipo a la "
            "WiFi del hospital (Ajustes > WiFi) y anota su direccion IP, que "
            "aparece en el informe de soporte.",
            "Updating the display firmware: 1) connect the device to the "
            "hospital WiFi (Settings > WiFi) and note its IP address, shown in "
            "the support report.",
            "Mettre a jour le firmware de l'ecran : 1) connectez l'appareil au "
            "WiFi de l'hopital (Reglages > WiFi) et notez son adresse IP, "
            "visible dans le rapport de support."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "2) Desde un ordenador en la misma red, abre http://<ip>/ en el "
            "navegador e introduce el usuario y la contrasena del equipo.",
            "2) From a computer on the same network, open http://<ip>/ in the "
            "browser and enter the device user and password.",
            "2) Depuis un ordinateur sur le meme reseau, ouvrez http://<ip>/ "
            "dans le navigateur et saisissez l'identifiant et le mot de passe "
            "de l'appareil."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "3) Elige el fichero firmware.bin de la pantalla y pulsa "
            "Actualizar. Espera a que la barra llegue al 100 %: la pantalla se "
            "reinicia sola. No apagues el equipo mientras.",
            "3) Choose the display firmware.bin file and press Update. Wait for "
            "the bar to reach 100%: the display restarts by itself. Do not "
            "power off meanwhile.",
            "3) Choisissez le fichier firmware.bin de l'ecran et Mettre a jour. "
            "Attendez que la barre atteigne 100 % : l'ecran redemarre seul. "
            "N'eteignez pas l'appareil pendant ce temps."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Si el equipo esta conectado al servidor de monitorizacion, las "
            "actualizaciones tambien pueden llegar solas desde alli. "
            "Comprueba la version en Ajustes > Informacion.",
            "If the device is connected to the monitoring server, updates can "
            "also arrive from there on their own. Check the version in "
            "Settings > Information.",
            "Si l'appareil est connecte au serveur de suivi, les mises a jour "
            "peuvent aussi arriver d'elles-memes. Verifiez la version dans "
            "Reglages > Informations."),
    QUIZ(&ui_ScreenMain, &QUIZ_T6,
         "Pregunta: como se actualiza el firmware de la pantalla?",
         "Question: how is the display firmware updated?",
         "Question : comment met-on a jour le firmware de l'ecran ?"),
};

// ---- T7: informe de soporte -------------------------------------------------------------

const Quiz QUIZ_T7 = {
    {T3("El nombre del bebe y su peso", "The baby's name and weight",
        "Le nom du bebe et son poids"),
     T3("Versiones, arranques, red, enlace, consignas, medidas, alarmas y "
        "memoria",
        "Versions, boots, network, link, setpoints, readings, alarms and "
        "memory",
        "Versions, demarrages, reseau, liaison, consignes, mesures, alarmes "
        "et memoire"),
     T3("La contrasena de la WiFi", "The WiFi password",
        "Le mot de passe WiFi")},
    1,
    T3("El informe lleva el estado tecnico del equipo, sin datos del paciente "
       "ni contrasenas. Con el, soporte puede empezar a diagnosticar sin "
       "preguntar.",
       "The report carries the device technical status, no patient data or "
       "passwords. With it, support can start diagnosing without asking.",
       "Le rapport porte l'etat technique de l'appareil, sans donnees "
       "patient ni mots de passe. Avec lui, le support peut diagnostiquer "
       "sans demander."),
};

const Step T7_STEPS[] = {
    DO(&ui_HelpButton, &ui_ScreenMain, goalHelpOpen,
       "Toca el boton de ayuda.", "Touch the help button.",
       "Touchez le bouton d'aide."),
    FREE(&ui_ScreenMain, goalHelpClosed,
         "Elige CONTACTAR SOPORTE. El QR abre un correo con el numero de serie "
         "en el asunto y el informe tecnico en el cuerpo. Con SIN INFORME el "
         "QR es mas ligero. Cierra con la X.",
         "Choose CONTACT SUPPORT. The QR opens an email with the serial number "
         "in the subject and the technical report in the body. NO REPORT gives "
         "a lighter QR. Close with the X.",
         "Choisissez CONTACTER LE SUPPORT. Le QR ouvre un e-mail avec le "
         "numero de serie en objet et le rapport technique dans le corps. SANS "
         "RAPPORT allege le QR. Fermez avec la X."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "El informe lleva versiones, arranques y motivo del ultimo reset, "
            "WiFi e IP, enlace con la placa, consignas, medidas, alarmas "
            "activas y memoria libre. Nunca datos del paciente.",
            "The report carries versions, boots and last reset reason, WiFi "
            "and IP, board link, setpoints, readings, active alarms and free "
            "memory. Never patient data.",
            "Le rapport porte versions, demarrages et motif du dernier reset, "
            "WiFi et IP, liaison carte, consignes, mesures, alarmes actives et "
            "memoire libre. Jamais de donnees patient."),
    QUIZ(&ui_ScreenMain, &QUIZ_T7,
         "Pregunta: que contiene el informe de soporte?",
         "Question: what does the support report contain?",
         "Question : que contient le rapport de support ?"),
};

// ---- T8: apagado seguro -------------------------------------------------------------------

const Quiz QUIZ_T8 = {
    {T3("Desenchufar directamente", "Unplug it directly",
        "Debrancher directement"),
     T3("Mantener el boton de encendido hasta completar la cuenta atras, con "
        "el bebe fuera",
        "Hold the power button until the countdown completes, with the baby "
        "out",
        "Maintenir le bouton marche/arret jusqu'a la fin du compte a "
        "rebours, bebe sorti"),
     T3("Bloquear la pantalla", "Lock the screen", "Verrouiller l'ecran")},
    1,
    T3("El boton fisico inicia una cuenta atras de 3 segundos que la pantalla "
       "muestra; soltarlo antes la cancela. Asi un roce no apaga la "
       "incubadora. Nunca con un bebe dentro.",
       "The physical button starts a 3-second countdown shown on screen; "
       "releasing it early cancels. So a brush cannot switch the incubator "
       "off. Never with a baby inside.",
       "Le bouton physique lance un compte a rebours de 3 secondes affiche a "
       "l'ecran ; relacher avant l'annule. Un frolement ne peut pas eteindre "
       "l'incubateur. Jamais avec un bebe dedans."),
};

const Step T8_STEPS[] = {
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Apagado seguro: con el bebe fuera y las terapias apagadas, "
            "manten pulsado el boton de encendido de la incubadora. La "
            "pantalla muestra una cuenta atras de 3 segundos.",
            "Safe shutdown: with the baby out and therapies off, hold the "
            "incubator power button. The screen shows a 3-second countdown.",
            "Arret sur : bebe sorti et therapies eteintes, maintenez le bouton "
            "marche/arret de l'incubateur. L'ecran affiche un compte a "
            "rebours de 3 secondes."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Si sueltas antes de que termine, se cancela: un roce no apaga el "
            "equipo. Al completarse, la pantalla se apaga y la placa corta la "
            "alimentacion.",
            "If you release before it ends, it cancels: a brush cannot switch "
            "the device off. When complete, the screen turns off and the "
            "board cuts power.",
            "Si vous relachez avant la fin, c'est annule : un frolement "
            "n'eteint pas l'appareil. A la fin, l'ecran s'eteint et la carte "
            "coupe l'alimentation."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "No desenchufes con el equipo encendido salvo emergencia: la placa "
            "guarda el estado al apagar de forma ordenada. Fin del curso "
            "tecnico.",
            "Do not unplug while on except in an emergency: the board saves "
            "its state on an orderly shutdown. End of the technician course.",
            "Ne debranchez pas l'appareil allume sauf urgence : la carte "
            "enregistre son etat lors d'un arret ordonne. Fin du cours "
            "technicien."),
    QUIZ(&ui_ScreenMain, &QUIZ_T8,
         "Pregunta: como se apaga la incubadora correctamente?",
         "Question: how is the incubator switched off correctly?",
         "Question : comment eteint-on correctement l'incubateur ?"),
};

#define LESSON(id, es, en, fr, steps)                                          \
  { id, T3(es, en, fr), steps, (uint8_t)(sizeof(steps) / sizeof(steps[0])), \
    LESSON_INTERACTIVE }

const Lesson TECH_LESSONS[] = {
    LESSON_INTRO,
    LESSON(1, "Informacion y versiones", "Information and versions",
           "Informations et versions", T1_STEPS),
    LESSON(2, "WiFi y servidor", "WiFi and server", "WiFi et serveur",
           T2_STEPS),
    LESSON(3, "Idioma y modos", "Language and modes", "Langue et modes",
           T3_STEPS),
    LESSON(4, "Ajustar la hora", "Setting the time", "Regler l'heure",
           T4_STEPS),
    LESSON(5, "Alarmas tecnicas", "Technical alarms", "Alarmes techniques",
           T5_STEPS),
    LESSON(6, "Actualizar el firmware", "Updating the firmware",
           "Mettre a jour le firmware", T6_STEPS),
    LESSON(7, "Informe de soporte", "Support report", "Rapport de support",
           T7_STEPS),
    LESSON(8, "Apagado seguro", "Safe shutdown", "Arret sur", T8_STEPS),
};

}  // namespace

const Course COURSE_TECH = {
    TRAINING_COURSE_TECH,
    T3("Tecnico", "Technician", "Technicien"),
    T3("Configuracion: informacion, WiFi, idioma, hora, alarmas tecnicas y "
       "actualizacion.",
       "Setup: information, WiFi, language, time, technical alarms and "
       "updates.",
       "Configuration : informations, WiFi, langue, heure, alarmes techniques "
       "et mise a jour."),
    TECH_LESSONS,
    (uint8_t)(sizeof(TECH_LESSONS) / sizeof(TECH_LESSONS[0])),
};

const Course *Training_CourseByIndex(uint8_t idx) {
  switch (idx) {
    case TRAINING_COURSE_NURSE: return &COURSE_NURSE;
    case TRAINING_COURSE_TECH: return &COURSE_TECH;
    default: return nullptr;
  }
}
