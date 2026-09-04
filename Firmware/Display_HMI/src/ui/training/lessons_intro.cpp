// Leccion 0 de ambos cursos: introduccion a la interfaz. Es el recorrido
// pasivo que antes vivia en HelpTour.cpp, ahora como tabla de pasos EXPLAIN.
#include "ui/training/lessons.h"

#include "ui.h"

// Textos ASCII sin acentos (las fuentes compiladas no tienen esos glifos).
static const Step INTRO_STEPS[] = {
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Bienvenido al tutorial de IncuNest. Te mostraremos cada control de "
            "la pantalla paso a paso. Pulsa SIGUIENTE para avanzar o SALIR en "
            "cualquier momento. Durante el recorrido no se acciona nada.",
            "Welcome to the IncuNest tutorial. We will show you each control on "
            "the screen step by step. Press NEXT to move on or EXIT at any time. "
            "Nothing is operated during the tour.",
            "Bienvenue dans le tutoriel IncuNest. Nous allons vous montrer chaque "
            "commande de l'ecran pas a pas. Appuyez sur SUIVANT pour avancer ou "
            "QUITTER a tout moment. Rien n'est actionne pendant la visite."),
    EXPLAIN(&ui_HelpButton, &ui_ScreenMain,
            "Boton de ayuda. Desde aqui abres los cursos de formacion, el codigo "
            "QR del video tutorial y el contacto con soporte tecnico.",
            "Help button. From here you open the training courses, the video "
            "tutorial QR code and the technical support contact.",
            "Bouton d'aide. D'ici vous ouvrez les cours de formation, le code QR "
            "du tutoriel video et le contact du support technique."),
    EXPLAIN(&ui_ClockButton, &ui_ScreenMain,
            "Hora y fecha del equipo. Tocalo para ajustar la hora a mano: de "
            "ella dependen las marcas de tiempo del historial clinico.",
            "Device time and date. Tap it to set the time manually: the "
            "clinical history timestamps depend on it.",
            "Heure et date de l'appareil. Touchez pour regler l'heure a la "
            "main : les horodatages de l'historique clinique en dependent."),
    EXPLAIN(&ui_ConnCont, &ui_ScreenMain,
            "Indicador de conectividad: WIFI o 2G y las barras de cobertura con "
            "el servidor de monitorizacion. Si se pierde el enlace con la placa "
            "de control, aparece tachado.",
            "Connectivity indicator: WIFI or 2G and the signal bars to the "
            "monitoring server. If the link with the control board is lost, "
            "it shows crossed out.",
            "Indicateur de connectivite : WIFI ou 2G et les barres de "
            "couverture vers le serveur de suivi. Si la liaison avec la carte "
            "de controle est perdue, il apparait barre."),
    EXPLAIN(&ui_ImgButton1, &ui_ScreenMain,
            "Candado. Un toque bloquea la pantalla para evitar toques "
            "accidentales. Tras 20 segundos sin tocar se bloquea sola; el "
            "anillo muestra el tiempo que falta.",
            "Padlock. One tap locks the screen to avoid accidental touches. "
            "After 20 seconds without touching it locks by itself; the ring "
            "shows the time left.",
            "Cadenas. Un appui verrouille l'ecran pour eviter les touches "
            "accidentelles. Apres 20 secondes sans contact il se verrouille "
            "seul ; l'anneau montre le temps restant."),
    EXPLAIN(&ui_BabiesButton, &ui_ScreenMain,
            "Bebes: alta de un nuevo paciente (nombre, semanas de gestacion, "
            "peso), historial de estancias y curva de peso de cada bebe.",
            "Babies: admit a new patient (name, gestational weeks, weight), "
            "stay history and weight curve of each baby.",
            "Bebes : admission d'un nouveau patient (nom, semaines de "
            "gestation, poids), historique des sejours et courbe de poids de "
            "chaque bebe."),
    EXPLAIN(&ui_AlarmButton, &ui_ScreenMain,
            "Alarmas: lista de las alarmas activas con la accion recomendada, y "
            "registro de las pasadas. El numero rojo indica cuantas hay "
            "activas.",
            "Alarms: list of active alarms with the recommended action, and "
            "log of past ones. The red number shows how many are active.",
            "Alarmes : liste des alarmes actives avec l'action recommandee, et "
            "journal des precedentes. Le chiffre rouge indique combien sont "
            "actives."),
    EXPLAIN(&ui_CheckImgMain, &ui_ScreenMain,
            "Este check significa que no hay ninguna alarma activa. Tocarlo "
            "tambien abre el registro de alarmas.",
            "This check mark means there is no active alarm. Tapping it also "
            "opens the alarm log.",
            "Cette coche signifie qu'aucune alarme n'est active. La toucher "
            "ouvre aussi le journal des alarmes."),
    EXPLAIN(&ui_TempCont, &ui_ScreenMain,
            "Control de temperatura. Elige AIRE (cabina) o PIEL (sonda en el "
            "bebe). Las flechas ajustan la consigna; las cifras grandes son la "
            "medida actual.",
            "Temperature control. Choose AIR (cabin) or SKIN (probe on the "
            "baby). The arrows adjust the setpoint; the big figures are the "
            "current reading.",
            "Controle de temperature. Choisissez AIR (habitacle) ou PEAU "
            "(sonde sur le bebe). Les fleches reglent la consigne ; les grands "
            "chiffres sont la mesure actuelle."),
    EXPLAIN(&ui_TempToggleBtn, &ui_ScreenMain,
            "Con este boton activas o desactivas el control de temperatura. Al "
            "activarlo se te pediran los datos del bebe si aun no estan.",
            "This button turns temperature control on or off. When turning it "
            "on you will be asked for the baby data if not yet entered.",
            "Ce bouton active ou desactive le controle de temperature. A "
            "l'activation, les donnees du bebe vous seront demandees si elles "
            "manquent."),
    EXPLAIN(&ui_HumCont, &ui_ScreenMain,
            "Control de humedad: la consigna con las flechas y el boton "
            "inferior para activarlo. Se puede ocultar desde Ajustes > Modos.",
            "Humidity control: setpoint with the arrows and the lower button "
            "to turn it on. It can be hidden from Settings > Modes.",
            "Controle d'humidite : la consigne avec les fleches et le bouton "
            "du bas pour l'activer. Il peut etre masque depuis Reglages > "
            "Modes."),
    EXPLAIN(&ui_PhotoCont, &ui_ScreenMain,
            "Fototerapia: fija los minutos con + y - e INICIAR arranca la "
            "cuenta atras. Cubre siempre los ojos del bebe antes de encender "
            "la luz.",
            "Phototherapy: set the minutes with + and -, START begins the "
            "countdown. Always cover the baby's eyes before switching the "
            "light on.",
            "Phototherapie : reglez les minutes avec + et -, DEMARRER lance le "
            "compte a rebours. Couvrez toujours les yeux du bebe avant "
            "d'allumer."),
    EXPLAIN(&ui_Settings, &ui_ScreenMain,
            "Ajustes: informacion del equipo, WiFi, idioma y modos de "
            "funcionamiento. Vamos a verlo.",
            "Settings: device information, WiFi, language and operating "
            "modes. Let's have a look.",
            "Reglages : informations sur l'appareil, WiFi, langue et modes de "
            "fonctionnement. Allons voir."),
    EXPLAIN(&ui_InfoCont, &ui_ScreenSettings,
            "Informacion: numero de serie y versiones de firmware de la "
            "pantalla y de la placa. Tenlo a mano al contactar con soporte.",
            "Information: serial number and firmware versions of the display "
            "and the board. Keep it handy when contacting support.",
            "Informations : numero de serie et versions du firmware de l'ecran "
            "et de la carte. Gardez-les a portee de main pour contacter le "
            "support."),
    EXPLAIN(&ui_WifiCont, &ui_ScreenSettings,
            "WiFi: red y contrasena para conectar el equipo al servidor de "
            "monitorizacion y recibir actualizaciones de firmware.",
            "WiFi: network and password to connect the device to the "
            "monitoring server and receive firmware updates.",
            "WiFi : reseau et mot de passe pour connecter l'appareil au "
            "serveur de suivi et recevoir les mises a jour du firmware."),
    EXPLAIN(&ui_LanguagesCont, &ui_ScreenSettings,
            "Idioma: espanol, ingles o frances. Se aplica al instante a toda la "
            "pantalla y a la placa de control.",
            "Language: Spanish, English or French. It applies at once to the "
            "whole screen and to the control board.",
            "Langue : espagnol, anglais ou francais. Elle s'applique aussitot a "
            "tout l'ecran et a la carte de controle."),
    EXPLAIN(&ui_ModesCont, &ui_ScreenSettings,
            "Modos: control por piel, modo oscuro y control de humedad. Lo que "
            "desactives aqui desaparece de la pantalla principal.",
            "Modes: skin control, dark mode and humidity control. Whatever you "
            "disable here disappears from the main screen.",
            "Modes : controle par la peau, mode sombre et controle d'humidite. "
            "Ce que vous desactivez ici disparait de l'ecran principal."),
    EXPLAIN(&ui_ImgButton2, &ui_ScreenSettings,
            "Con esta flecha vuelves a la pantalla principal.",
            "This arrow takes you back to the main screen.",
            "Cette fleche vous ramene a l'ecran principal."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Fin de la introduccion. Las siguientes lecciones son practicas: "
            "tocaras los controles reales con la incubadora en modo formacion.",
            "End of the introduction. The next lessons are hands-on: you will "
            "touch the real controls with the incubator in training mode.",
            "Fin de l'introduction. Les lecons suivantes sont pratiques : vous "
            "toucherez les vraies commandes, l'incubateur etant en mode "
            "formation."),
};

const Lesson LESSON_INTRO = {
    0,
    T3("Conoce la pantalla", "Meet the screen", "Decouvrir l'ecran"),
    INTRO_STEPS,
    (uint8_t)(sizeof(INTRO_STEPS) / sizeof(INTRO_STEPS[0])),
    0,
};
