// Curso de Enfermeria (spec hmi-training-courses). Orden pedido por el
// usuario: E0 intro, E1 alta y seguimiento del bebe, E2 temperatura por aire,
// E3 piel y sonda (solo si el modo piel esta habilitado en Ajustes), E4
// humedad (solo si el control de humedad esta habilitado), E5 fototerapia, E6
// atender una alarma, E7 salida del bebe, E8 bloqueo, E9 tendencia, E10 hora,
// E11 soporte.
//
// Todas las lecciones salvo la intro son LESSON_INTERACTIVE: aunque bloquear
// la pantalla o abrir la ayuda no cambie nada en la placa, el sandbox activo
// es lo unico que hace legitimo un paso "hacer" (design decision 2).
#include "ui/training/lessons.h"

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "state/training_mode.h"
#include "ui.h"
#include "ui/AlarmCenter.h"
#include "ui/BabyExitDialog.h"
#include "ui/BabyHistory.h"
#include "ui/BabyWizard.h"
#include "ui/HelpDialog.h"
#include "ui/TelemetryHistory.h"
#include "ui/TimeDialog.h"

extern ui_lang_t g_lang;

namespace {

// ---- Objetivos (se evaluan por estado en cada vuelta de UI) -------------------

UiControlSnapshot snap() {
  UiControlSnapshot s;
  UI_GetControlSnapshot(&s);
  return s;
}

bool visible(lv_obj_t *o) { return o && lv_obj_is_visible(o); }

// Disponibilidad de lecciones ligadas a opciones de Ajustes > Modos.
bool availSkin() { return snap().skinPanelEnabled; }
bool availHum() { return snap().humidityEnabled; }

// Temperatura y asistente
bool goalWizardOpen() { return BabyWizard_IsOpen(); }
bool goalTempOn() { return snap().switchTemp && !BabyWizard_IsOpen(); }
bool goalTempOff() { return !snap().switchTemp && !BabyWizard_IsOpen(); }
// Fases del asistente, para explicar cada pantalla mientras el alumno la
// tiene delante (pasos libres encadenados).
bool goalWizardAtWeight() { return BabyWizard_GetStep() == BW_WEIGHT; }
bool goalWizardAtAge() { return BabyWizard_GetStep() == BW_AGE; }
bool goalWizardAtSummary() { return BabyWizard_GetStep() == BW_SUMMARY; }

// Los objetivos "sube la consigna" aceptan tambien el tope del rango: si el
// equipo ya estaba al maximo, el paso no puede quedarse sin salida.
double s_airBase = 0.0;
void enterAirBase() { s_airBase = snap().airTempValue; }
bool goalAirUpTwo() {
  const double a = snap().airTempValue;
  return a >= s_airBase + 0.39 || a >= AIR_TEMP_MAX - 0.01;
}

// Piel. La sonda es real (la placa la detecta): sin sonda el paso de
// seleccionar PIEL se da por cumplido para que la leccion no se quede colgada.
bool probeOk() { return g_skinProbeState == SKIN_PROBE_VALID; }
bool goalSkinSelectedOrNoProbe() {
  return snap().selectedPanel == SKIN_PANEL_SELECTED || !probeOk();
}

// Humedad
bool goalHumOnOrWizard() { return snap().switchHum || BabyWizard_IsOpen(); }
bool goalHumOn() { return snap().switchHum && !BabyWizard_IsOpen(); }
bool goalHumOff() { return !snap().switchHum; }
int s_humBase = 0;
void enterHumBase() { s_humBase = snap().humValue; }
bool goalHumUp() {
  const int h = snap().humValue;
  return h >= s_humBase + 5 || h >= HUM_MAX;
}

// Fototerapia
bool goalPhotoOnOrDialog() {
  return hmi_msg.phototherapyMode == PHOTOTHERAPY_ON || BabyWizard_IsOpen() ||
         visible(ui_PhotoSafetyOverlay);
}
bool goalPhotoOn() {
  return hmi_msg.phototherapyMode == PHOTOTHERAPY_ON && !BabyWizard_IsOpen() &&
         !visible(ui_PhotoSafetyOverlay);
}
bool goalPhotoOff() { return hmi_msg.phototherapyMode == PHOTOTHERAPY_OFF; }

// Alarmas
bool goalAlarmCenterOpen() { return AlarmCenter_IsOpen(); }
bool goalAlarmCenterClosed() { return !AlarmCenter_IsOpen(); }

// Bebes. En formacion el asistente solo ofrece a ZOE y rechaza BEBE NUEVO y
// SALTAR (BabyWizard::trainingRefuse), asi que el unico camino que enciende
// el control deja seq = TRAINING_BABY_SEQ.
bool goalBabyHistoryOpen() { return BabyHistory_IsOpen(); }
bool goalBabyHistoryClosed() { return !BabyHistory_IsOpen(); }
bool goalTrainingBabyAdmitted() {
  return snap().switchTemp && !BabyWizard_IsOpen() &&
         BabyWizard_GetActiveSeq() == TRAINING_BABY_SEQ;
}
// Salida: el dialogo se permite solo en este paso, y el objetivo es haberlo
// visto abierto y despues cerrado con el control apagado.
bool s_exitSeen = false;
void enterExitStep() {
  s_exitSeen = false;
  Training_SetExitDialogAllowed(true);
}
bool goalExitDone() {
  if (BabyExitDialog_IsOpen()) s_exitSeen = true;
  return s_exitSeen && !BabyExitDialog_IsOpen() && !snap().switchTemp;
}

// Bloqueo. Por pantalla activa, nunca por la global `locked`: esa vale true en
// la principal casi siempre (unlock_timeout_cb la rearma 5 s despues de
// desbloquear) y false en cuanto aparece el pop-up de desbloqueo, asi que
// como objetivo daria los dos gestos por hechos sin hacerlos.
bool goalLocked() {
  return lv_scr_act() == ui_ScreenLock && !visible(ui_UnlockCont);
}
bool goalUnlockPopup() {
  return lv_scr_act() == ui_ScreenLock && visible(ui_UnlockCont);
}
bool goalUnlocked() { return lv_scr_act() == ui_ScreenMain; }

// Tendencia, hora, soporte
bool goalTrendOpen() { return TelemetryHistory_IsOpen(); }
bool goalTrendClosed() { return !TelemetryHistory_IsOpen(); }
bool goalTimeOpen() { return TimeDialog_IsOpen(); }
bool goalTimeClosed() { return !TimeDialog_IsOpen(); }
bool goalHelpOpen() { return HelpDialog_IsOpen(); }
bool goalHelpClosed() { return !HelpDialog_IsOpen(); }

// ---- E1: alta y seguimiento del bebe ------------------------------------------

const Quiz QUIZ_BABY = {
    {T3("Solo para mostrar el nombre en pantalla",
        "Only to show the name on screen",
        "Seulement pour afficher le nom a l'ecran"),
     T3("Para proponer la temperatura de aire y registrar la estancia",
        "To propose the air temperature and record the stay",
        "Pour proposer la temperature d'air et enregistrer le sejour"),
     T3("Para conectarse a la red WiFi", "To connect to the WiFi network",
        "Pour se connecter au reseau WiFi")},
    1,
    T3("Con peso, semanas y edad el equipo calcula el rango de temperatura "
       "neutra y propone una consigna. Ademas cada bebe tiene su registro: "
       "pesos, horas de terapia, salidas.",
       "With weight, weeks and age the device computes the neutral "
       "temperature range and proposes a setpoint. Each baby also has a "
       "record: weights, therapy hours, exits.",
       "Avec poids, semaines et age, l'appareil calcule la plage de "
       "temperature neutre et propose une consigne. Chaque bebe a aussi son "
       "dossier : poids, heures de therapie, sorties."),
};

const Step BABY_STEPS[] = {
    EXPLAIN(&ui_BabiesButton, &ui_ScreenMain,
            "Cada bebe tiene su registro: nombre, semanas, pesos, horas de "
            "terapia y salidas. Se crea con BEBE NUEVO al encender una terapia "
            "por primera vez. En formacion practicaras con ZOE, ya creada.",
            "Each baby has a record: name, weeks, weights, therapy hours and "
            "exits. It is created with NEW BABY when a therapy is first "
            "switched on. In training you practise with ZOE, already created.",
            "Chaque bebe a son dossier : nom, semaines, poids, heures de "
            "therapie et sorties. Il se cree avec NOUVEAU BEBE a la premiere "
            "therapie. En formation vous pratiquez avec ZOE, deja creee."),
    DO(&ui_BabiesButton, &ui_ScreenMain, goalBabyHistoryOpen,
       "Toca Bebes para ver los registros. Dentro veras el bebe activo, los "
       "ya dados de alta y la curva de peso de cada uno. Cierra con la X al "
       "terminar.",
       "Touch Babies to see the records. Inside you see the active baby, the "
       "discharged ones and each weight curve. Close with the X when done.",
       "Touchez Bebes pour voir les dossiers : le bebe actif, ceux sortis et "
       "la courbe de poids de chacun. Fermez avec la X a la fin."),
    FREE(&ui_ScreenMain, goalBabyHistoryClosed,
         "Explora los registros de bebes. Cierra con la X para continuar.",
         "Explore the baby records. Close with the X to continue.",
         "Explorez les dossiers des bebes. Fermez avec la X pour continuer."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalWizardOpen,
       "El registro se abre desde cualquier terapia. Enciende el control de "
       "temperatura para abrir el asistente del bebe.",
       "The record is opened from any therapy. Turn temperature control on to "
       "open the baby assistant.",
       "Le dossier s'ouvre depuis n'importe quelle therapie. Allumez le "
       "controle de temperature pour ouvrir l'assistant bebe."),
    FREE(&ui_ScreenMain, goalTrainingBabyAdmitted,
         "Selecciona a ZOE. Introduce su peso de hoy en gramos y sus dias de "
         "vida: asi se registra un peso nuevo. Lee la temperatura propuesta y "
         "pulsa APLICAR. ZOE no se guarda en el historial.",
         "Select ZOE. Enter today's weight in grams and her days of life: "
         "that is how a new weight is recorded. Read the proposed temperature "
         "and press APPLY. ZOE is not saved to the history.",
         "Selectionnez ZOE. Saisissez son poids du jour en grammes et ses "
         "jours de vie : c'est ainsi qu'un poids s'enregistre. Lisez la "
         "temperature proposee et APPLIQUER. ZOE n'est pas gardee."),
    EXPLAIN(&ui_BabiesButton, &ui_ScreenMain,
            "Cada nuevo peso se registra desde el asistente y dibuja la curva "
            "de crecimiento en Bebes. Registra el peso a diario a la misma "
            "hora.",
            "Each new weight is recorded from the assistant and draws the "
            "growth curve in Babies. Record the weight daily at the same "
            "time.",
            "Chaque nouveau poids s'enregistre depuis l'assistant et trace la "
            "courbe de croissance dans Bebes. Pesez chaque jour a la meme "
            "heure."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalTempOff,
       "Apaga el control de temperatura para terminar.",
       "Turn temperature control off to finish.",
       "Eteignez le controle de temperature pour terminer."),
    QUIZ(&ui_ScreenMain, &QUIZ_BABY,
         "Pregunta: para que pide el equipo los datos del bebe?",
         "Question: why does the device ask for the baby data?",
         "Question : pourquoi l'appareil demande-t-il les donnees du bebe ?"),
};

// ---- E2: temperatura por aire ----------------------------------------------

const Quiz QUIZ_AIR = {
    {T3("La temperatura medida ahora en la cabina",
        "The temperature measured now in the cabin",
        "La temperature mesuree maintenant dans l'habitacle"),
     T3("La consigna que has fijado", "The setpoint you have set",
        "La consigne que vous avez fixee"),
     T3("La temperatura del bebe", "The baby's temperature",
        "La temperature du bebe")},
    0,
    T3("La cifra grande es siempre la MEDIDA actual. La consigna es la cifra "
       "pequena junto a las flechas; la del bebe solo se ve en PIEL con la "
       "sonda puesta.",
       "The big figure is always the current READING. The setpoint is the "
       "small figure next to the arrows; the baby's temperature is only "
       "shown in SKIN mode with the probe on.",
       "Le grand chiffre est toujours la MESURE actuelle. La consigne est le "
       "petit chiffre pres des fleches ; celle du bebe ne se voit qu'en PEAU "
       "avec la sonde."),
};

const Step AIR_STEPS[] = {
    EXPLAIN(&ui_TempCont, &ui_ScreenMain,
            "Vamos a encender el control de temperatura por aire. Estamos en "
            "modo formacion: la incubadora actua de verdad, pero el bebe ZOE "
            "es de practica y nada queda registrado.",
            "Let's turn on air temperature control. We are in training mode: "
            "the incubator really acts, but baby ZOE is a practice baby and "
            "nothing is recorded.",
            "Allumons le controle de temperature par air. Nous sommes en mode "
            "formation : l'incubateur agit vraiment, mais le bebe ZOE est un "
            "bebe d'exercice et rien n'est enregistre."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalWizardOpen,
       "Toca el boton de encendido del control de temperatura. Se abrira el "
       "asistente del bebe: antes de calentar, el equipo necesita saber a "
       "quien esta cuidando.",
       "Touch the temperature control power button. The baby assistant will "
       "open: before heating, the device needs to know who it is caring for.",
       "Touchez le bouton de mise en marche du controle de temperature. "
       "L'assistant bebe s'ouvre : avant de chauffer, l'appareil doit savoir "
       "de qui il s'occupe."),
    // Un paso libre por pantalla del asistente: la explicacion se lee con la
    // pantalla delante y avanza cuando el alumno pasa a la siguiente.
    FREE(&ui_ScreenMain, goalWizardAtWeight,
         "1/4 Bebe: la lista muestra los bebes registrados. Selecciona a ZOE. "
         "Con un bebe real nuevo usarias BEBE NUEVO (nombre y semanas de "
         "gestacion).",
         "1/4 Baby: the list shows the registered babies. Select ZOE. With a "
         "real new baby you would use NEW BABY (name and gestational weeks).",
         "1/4 Bebe : la liste montre les bebes enregistres. Selectionnez ZOE. "
         "Pour un vrai nouveau bebe : NOUVEAU BEBE (nom et semaines de "
         "gestation)."),
    FREE(&ui_ScreenMain, goalWizardAtAge,
         "2/4 Peso: introduce el peso de hoy en gramos. El peso y las semanas "
         "de gestacion definen cuanto calor necesita el bebe para no gastar "
         "energia en calentarse.",
         "2/4 Weight: enter today's weight in grams. Weight and gestational "
         "weeks define how much warmth the baby needs to avoid spending energy "
         "on keeping warm.",
         "2/4 Poids : saisissez le poids du jour en grammes. Poids et semaines "
         "de gestation definissent la chaleur dont le bebe a besoin pour ne "
         "pas depenser d'energie."),
    FREE(&ui_ScreenMain, goalWizardAtSummary,
         "3/4 Dias de vida: la temperatura neutra baja a medida que el bebe "
         "crece. Introduce los dias desde el nacimiento y pulsa CONTINUAR.",
         "3/4 Days of life: the neutral temperature falls as the baby grows. "
         "Enter the days since birth and press CONTINUE.",
         "3/4 Jours de vie : la temperature neutre baisse a mesure que le bebe "
         "grandit. Saisissez les jours depuis la naissance et CONTINUER."),
    FREE(&ui_ScreenMain, goalTempOn,
         "4/4 Resumen: con peso, semanas y dias el equipo calcula el rango de "
         "temperatura neutra y propone una consigna de aire. APLICAR la fija y "
         "enciende el control.",
         "4/4 Summary: with weight, weeks and days the device computes the "
         "neutral temperature range and proposes an air setpoint. APPLY sets "
         "it and turns the control on.",
         "4/4 Resume : avec poids, semaines et jours l'appareil calcule la "
         "plage de temperature neutre et propose une consigne d'air. APPLIQUER "
         "la fixe et allume le controle."),
    EXPLAIN(&ui_AirPanel, &ui_ScreenMain,
            "El control ha quedado en AIRE con la consigna propuesta: regula "
            "por la temperatura de la cabina. PIEL usa la sonda sobre el bebe.",
            "Control is now in AIR with the proposed setpoint: it regulates on "
            "cabin temperature. SKIN uses the probe on the baby.",
            "Le controle est en AIR avec la consigne proposee : il regule sur "
            "la temperature de l'habitacle. PEAU utilise la sonde sur le bebe."),
    DO_ENTER(&ui_ImgArrowUpTemp, &ui_ScreenMain, goalAirUpTwo, enterAirBase,
             "Si el medico indica otra consigna, se ajusta con las flechas. "
             "Subela dos pasos (0,2 grados cada toque).",
             "If the doctor indicates another setpoint, adjust it with the "
             "arrows. Raise it two steps (0.2 degrees per touch).",
             "Si le medecin indique une autre consigne, reglez-la avec les "
             "fleches. Montez-la de deux pas (0,2 degre par appui)."),
    EXPLAIN(&ui_AirPanel, &ui_ScreenMain,
            "La cifra grande es la temperatura medida ahora; la pequena junto "
            "a las flechas es tu consigna. La incubadora calienta hasta "
            "igualarlas: fijate en la barra de potencia del calefactor.",
            "The big figure is the temperature measured now; the small one "
            "next to the arrows is your setpoint. The incubator heats until "
            "they match: watch the heater power bar.",
            "Le grand chiffre est la temperature mesuree ; le petit pres des "
            "fleches est votre consigne. L'incubateur chauffe jusqu'a les "
            "egaler : observez la barre de puissance du chauffage."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalTempOff,
       "Apaga el control de temperatura con el mismo boton.",
       "Turn temperature control off with the same button.",
       "Eteignez le controle de temperature avec le meme bouton."),
    QUIZ(&ui_ScreenMain, &QUIZ_AIR,
         "Pregunta: en el panel AIRE, la cifra grande es...",
         "Question: in the AIR panel, the big figure is...",
         "Question : dans le panneau AIR, le grand chiffre est..."),
};

// ---- E3: control por piel y sonda (solo con el modo piel habilitado) -----------

const Quiz QUIZ_SKIN = {
    {T3("Que el bebe tenga menos de 1500 g", "The baby weighing under 1500 g",
        "Que le bebe pese moins de 1500 g"),
     T3("Que la sonda de piel este conectada y bien colocada",
        "The skin probe connected and well placed",
        "Que la sonde cutanee soit branchee et bien placee"),
     T3("Que el control de humedad este activo", "Humidity control being on",
        "Que le controle d'humidite soit actif")},
    1,
    T3("El modo piel regula por la temperatura del bebe: sin sonda conectada "
       "el equipo lo rechaza y avisa. La consigna de piel es fija, 36,5 "
       "grados.",
       "Skin mode regulates on the baby's temperature: without a connected "
       "probe the device refuses it and warns. The skin setpoint is fixed at "
       "36.5 degrees.",
       "Le mode peau regule sur la temperature du bebe : sans sonde branchee "
       "l'appareil refuse et avertit. La consigne peau est fixe, 36,5 "
       "degres."),
};

const Step SKIN_STEPS[] = {
    EXPLAIN(&ui_TempCont, &ui_ScreenMain,
            "El modo PIEL regula por la temperatura del bebe, medida con la "
            "sonda pegada a su piel. Esta habilitado en Ajustes > Modos; hace "
            "falta la sonda conectada.",
            "SKIN mode regulates on the baby's temperature, measured by the "
            "probe stuck to the skin. It is enabled in Settings > Modes; the "
            "probe must be connected.",
            "Le mode PEAU regule sur la temperature du bebe, mesuree par la "
            "sonde collee a la peau. Il est active dans Reglages > Modes ; la "
            "sonde doit etre branchee."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalWizardOpen,
       "Enciende el control de temperatura.",
       "Turn temperature control on.",
       "Allumez le controle de temperature."),
    FREE(&ui_ScreenMain, goalTempOn,
         "Selecciona a ZOE, introduce peso y dias de vida y pulsa APLICAR.",
         "Select ZOE, enter weight and days of life and press APPLY.",
         "Selectionnez ZOE, saisissez poids et jours de vie et APPLIQUER."),
    DO(&ui_SkinPanel, &ui_ScreenMain, goalSkinSelectedOrNoProbe,
       "Toca PIEL para regular por la temperatura del bebe. Sin sonda "
       "conectada este paso se salta.",
       "Touch SKIN to regulate on the baby's temperature. Without a connected "
       "probe this step is skipped.",
       "Touchez PEAU pour reguler sur la temperature du bebe. Sans sonde "
       "branchee cette etape est sautee."),
    EXPLAIN(&ui_SkinPanel, &ui_ScreenMain,
            "En PIEL la consigna es fija: 36,5 grados sobre la piel del bebe, "
            "sin flechas. Coloca la sonda en el abdomen, lejos del higado, y "
            "fijala con el adhesivo.",
            "In SKIN the setpoint is fixed: 36.5 degrees on the baby's skin, "
            "no arrows. Place the probe on the abdomen, away from the liver, "
            "and fix it with the adhesive.",
            "En PEAU la consigne est fixe : 36,5 degres sur la peau du bebe, "
            "sans fleches. Placez la sonde sur l'abdomen, loin du foie, et "
            "fixez-la avec l'adhesif."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "Si la sonda se despega, el equipo avisa y pasa a proteger al "
            "bebe: revisa la sonda antes que nada. La temperatura de la "
            "cabina sigue visible en el panel AIRE.",
            "If the probe comes off, the device warns and protects the baby: "
            "check the probe first of all. Cabin temperature stays visible in "
            "the AIR panel.",
            "Si la sonde se decolle, l'appareil avertit et protege le bebe : "
            "verifiez la sonde avant tout. La temperature de l'habitacle "
            "reste visible dans le panneau AIR."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalTempOff,
       "Apaga el control de temperatura.",
       "Turn temperature control off.",
       "Eteignez le controle de temperature."),
    QUIZ(&ui_ScreenMain, &QUIZ_SKIN,
         "Pregunta: que hace falta para usar el modo PIEL?",
         "Question: what is needed to use SKIN mode?",
         "Question : que faut-il pour utiliser le mode PEAU ?"),
};

// ---- E4: humedad (solo con el control de humedad habilitado) ---------------------

const Quiz QUIZ_HUM = {
    {T3("Sube la temperatura del aire", "It raises the air temperature",
        "Elle augmente la temperature de l'air"),
     T3("Reduce la perdida de agua y calor por la piel del bebe",
        "It reduces water and heat loss through the baby's skin",
        "Elle reduit la perte d'eau et de chaleur par la peau du bebe"),
     T3("Limpia el aire de la cabina", "It cleans the cabin air",
        "Elle nettoie l'air de l'habitacle")},
    1,
    T3("La humedad alta reduce la evaporacion por la piel, muy fina en "
       "prematuros, y con ella la perdida de agua y de calor. Se ajusta por "
       "peso y edad.",
       "High humidity reduces evaporation through the skin, very thin in "
       "preterm babies, and with it water and heat loss. It is set by weight "
       "and age.",
       "L'humidite elevee reduit l'evaporation par la peau, tres fine chez "
       "les prematures, et avec elle la perte d'eau et de chaleur. Elle se "
       "regle selon poids et age."),
};

const Step HUM_STEPS[] = {
    EXPLAIN(&ui_HumCont, &ui_ScreenMain,
            "Control de humedad. La cifra grande es la humedad medida en la "
            "cabina; la pequena, tu consigna.",
            "Humidity control. The big figure is the humidity measured in the "
            "cabin; the small one, your setpoint.",
            "Controle d'humidite. Le grand chiffre est l'humidite mesuree ; "
            "le petit, votre consigne."),
    DO(&ui_HumToggleBtn, &ui_ScreenMain, goalHumOnOrWizard,
       "Toca el boton de encendido de la humedad. Se abrira el asistente "
       "para identificar al bebe.",
       "Touch the humidity power button. The assistant opens to identify the "
       "baby.",
       "Touchez le bouton de mise en marche de l'humidite. L'assistant "
       "s'ouvre pour identifier le bebe."),
    FREE(&ui_ScreenMain, goalHumOn,
         "Selecciona a ZOE en la lista. Al terminar, la humedad queda "
         "encendida.",
         "Select ZOE in the list. When done, humidity is on.",
         "Selectionnez ZOE dans la liste. A la fin, l'humidite est allumee."),
    DO_ENTER(&ui_ArrowUpHum, &ui_ScreenMain, goalHumUp, enterHumBase,
             "Sube la consigna de humedad un paso con la flecha (5 % cada "
             "toque).",
             "Raise the humidity setpoint one step with the arrow (5% per "
             "touch).",
             "Montez la consigne d'humidite d'un pas avec la fleche (5 % par "
             "appui)."),
    EXPLAIN(&ui_HumCont, &ui_ScreenMain,
            "La incubadora humidifica hasta alcanzar la consigna. Vigila el "
            "deposito de agua: sin agua no puede humidificar y avisara.",
            "The incubator humidifies until the setpoint is reached. Watch "
            "the water tank: without water it cannot humidify and will warn.",
            "L'incubateur humidifie jusqu'a la consigne. Surveillez le "
            "reservoir d'eau : sans eau il ne peut pas humidifier et "
            "avertira."),
    DO(&ui_HumToggleBtn, &ui_ScreenMain, goalHumOff,
       "Apaga la humedad con el mismo boton.",
       "Turn humidity off with the same button.",
       "Eteignez l'humidite avec le meme bouton."),
    QUIZ(&ui_ScreenMain, &QUIZ_HUM,
         "Pregunta: para que sirve la humedad en la incubadora?",
         "Question: what is humidity for in the incubator?",
         "Question : a quoi sert l'humidite dans l'incubateur ?"),
};

// ---- E5: fototerapia segura -----------------------------------------------------

const Quiz QUIZ_PHOTO = {
    {T3("Cubrir los ojos del bebe", "Covering the baby's eyes",
        "Couvrir les yeux du bebe"),
     T3("Subir la temperatura del aire", "Raising the air temperature",
        "Augmenter la temperature de l'air"),
     T3("Apagar la humedad", "Turning humidity off",
        "Eteindre l'humidite")},
    0,
    T3("La luz de fototerapia dana la retina: los ojos del bebe deben estar "
       "cubiertos ANTES de encenderla. Por eso el equipo lo pregunta cada "
       "vez.",
       "Phototherapy light damages the retina: the baby's eyes must be "
       "covered BEFORE switching it on. That is why the device asks every "
       "time.",
       "La lumiere de phototherapie abime la retine : les yeux du bebe "
       "doivent etre couverts AVANT de l'allumer. C'est pourquoi l'appareil "
       "le demande chaque fois."),
};

const Step PHOTO_STEPS[] = {
    EXPLAIN(&ui_PhotoCont, &ui_ScreenMain,
            "Fototerapia. Aqui la enciendes y, si quieres, programas los "
            "minutos de tratamiento. Recuerda: la luz es perjudicial para los "
            "ojos, hay que cubrirlos siempre antes.",
            "Phototherapy. Here you switch it on and, if you want, set the "
            "treatment minutes. Remember: the light harms the eyes, always "
            "cover them first.",
            "Phototherapie. Ici vous l'allumez et, si vous voulez, programmez "
            "les minutes de traitement. Rappel : la lumiere abime les yeux, "
            "couvrez-les toujours avant."),
    DO(&ui_PhotoToggleBtn, &ui_ScreenMain, goalPhotoOnOrDialog,
       "Toca el boton de encendido de la fototerapia.",
       "Touch the phototherapy power button.",
       "Touchez le bouton de mise en marche de la phototherapie."),
    FREE(&ui_ScreenMain, goalPhotoOn,
         "Selecciona a ZOE en la lista. Despues, en el aviso de proteccion "
         "ocular, confirma que los ojos estan cubiertos: la lampara se "
         "encendera de verdad.",
         "Select ZOE in the list. Then, in the eye protection notice, confirm "
         "the eyes are covered: the lamp will really switch on.",
         "Selectionnez ZOE dans la liste. Puis, dans l'avis de protection "
         "oculaire, confirmez que les yeux sont couverts : la lampe "
         "s'allumera vraiment."),
    EXPLAIN(&ui_PhotoTimerCont, &ui_ScreenMain,
            "La lampara esta encendida. Si quieres, programa un temporizador: "
            "minutos con + y -, INICIAR, y la lampara se apaga sola al "
            "terminar. Sin temporizador sigue encendida hasta que la apagues.",
            "The lamp is on. If you want, set a timer: minutes with + and -, "
            "START, and the lamp switches off by itself at the end. Without a "
            "timer it stays on until you switch it off.",
            "La lampe est allumee. Si vous voulez, programmez un minuteur : "
            "minutes avec + et -, DEMARRER, et la lampe s'eteint seule a la "
            "fin. Sans minuteur elle reste allumee jusqu'a l'arret."),
    DO(&ui_PhotoToggleBtn, &ui_ScreenMain, goalPhotoOff,
       "Apaga la fototerapia con el boton de encendido.",
       "Turn phototherapy off with the power button.",
       "Eteignez la phototherapie avec le bouton marche/arret."),
    QUIZ(&ui_ScreenMain, &QUIZ_PHOTO,
         "Pregunta: que hay que hacer SIEMPRE antes de encender la "
         "fototerapia?",
         "Question: what must ALWAYS be done before switching phototherapy "
         "on?",
         "Question : que faut-il TOUJOURS faire avant d'allumer la "
         "phototherapie ?"),
};

// ---- E6: atender una alarma -------------------------------------------------

const Quiz QUIZ_ALARM = {
    {T3("Apaga la alarma para siempre", "Turns the alarm off for good",
        "Eteint l'alarme definitivement"),
     T3("Silencia el sonido un tiempo; la alarma sigue activa y vuelve a "
        "sonar sola",
        "Mutes the sound for a while; the alarm stays active and sounds "
        "again by itself",
        "Coupe le son un moment ; l'alarme reste active et resonne seule"),
     T3("Resuelve la causa de la alarma", "Fixes the cause of the alarm",
        "Corrige la cause de l'alarme")},
    1,
    T3("La pausa de audio solo calla el sonido durante un tiempo limitado. "
       "La condicion sigue activa y el sonido vuelve solo si no se ha "
       "resuelto.",
       "Audio pause only mutes the sound for a limited time. The condition "
       "stays active and the sound comes back by itself if not resolved.",
       "La pause audio ne coupe le son que pour un temps limite. La condition "
       "reste active et le son revient seul si elle n'est pas resolue."),
};

const Step ALARM_STEPS[] = {
    EXPLAIN(&ui_AlarmButton, &ui_ScreenMain,
            "Cuando suena una alarma, lo primero es leer QUE pasa y QUE hacer. "
            "Todo esta en el centro de alarmas.",
            "When an alarm sounds, first read WHAT is happening and WHAT to "
            "do. It is all in the alarm center.",
            "Quand une alarme sonne, lisez d'abord CE QUI se passe et QUE "
            "faire. Tout est dans le centre d'alarmes."),
    DO(&ui_AlarmButton, &ui_ScreenMain, goalAlarmCenterOpen,
       "Toca el icono de alarmas. Dentro, cada alarma muestra su titulo y la "
       "accion recomendada; el boton de pausa silencia el sonido un tiempo. "
       "Cuando lo hayas visto, cierra con la X.",
       "Touch the alarm icon. Inside, each alarm shows its title and the "
       "recommended action; the pause button mutes the sound for a while. "
       "When done, close with the X.",
       "Touchez l'icone d'alarmes. Chaque alarme y montre son titre et "
       "l'action recommandee ; le bouton pause coupe le son un moment. "
       "Ensuite, fermez avec la X."),
    FREE(&ui_ScreenMain, goalAlarmCenterClosed,
         "Lee la lista de alarmas y la accion recomendada. Cierra el centro "
         "de alarmas con la X para continuar.",
         "Read the alarm list and the recommended action. Close the alarm "
         "center with the X to continue.",
         "Lisez la liste des alarmes et l'action recommandee. Fermez le centre "
         "d'alarmes avec la X pour continuer."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "La pausa de audio silencia el sonido durante un tiempo limitado; "
            "la alarma sigue activa hasta que se resuelve su causa, y el "
            "sonido vuelve solo al acabar la pausa.",
            "Audio pause mutes the sound for a limited time; the alarm stays "
            "active until its cause is resolved, and the sound comes back by "
            "itself when the pause ends.",
            "La pause audio coupe le son pour un temps limite ; l'alarme reste "
            "active jusqu'a la resolution de sa cause, et le son revient seul "
            "a la fin de la pause."),
    DO(&ui_CheckImgMain, &ui_ScreenMain, goalAlarmCenterOpen,
       "Sin alarmas, el check verde ocupa su sitio. Tocalo: tambien abre el "
       "registro de las alarmas pasadas. Luego cierra con la X.",
       "With no alarms, the green check takes their place. Touch it: it also "
       "opens the log of past alarms. Then close with the X.",
       "Sans alarme, la coche verte prend leur place. Touchez-la : elle ouvre "
       "aussi le journal des alarmes passees. Puis fermez avec la X."),
    FREE(&ui_ScreenMain, goalAlarmCenterClosed,
         "Este es el registro de alarmas pasadas. Cierra con la X para "
         "continuar.",
         "This is the log of past alarms. Close with the X to continue.",
         "Voici le journal des alarmes passees. Fermez avec la X pour "
         "continuer."),
    QUIZ(&ui_ScreenMain, &QUIZ_ALARM,
         "Pregunta: que hace el boton de pausa de audio?",
         "Question: what does the audio pause button do?",
         "Question : que fait le bouton de pause audio ?"),
};

// ---- E7: salida del bebe ---------------------------------------------------------

const Quiz QUIZ_EXIT = {
    {T3("Es lo mismo que el alta", "It is the same as discharge",
        "C'est la meme chose que la sortie"),
     T3("El bebe sale un rato con la madre y su registro sigue activo",
        "The baby goes out with the mother for a while and the record stays "
        "active",
        "Le bebe sort un moment avec la mere et son dossier reste actif"),
     T3("Se borra el registro del bebe", "The baby record is deleted",
        "Le dossier du bebe est efface")},
    1,
    T3("Metodo canguro: el bebe sale con la madre y vuelve. El registro sigue "
       "activo y se cuenta la salida. El alta cierra el registro con su "
       "resultado.",
       "Kangaroo care: the baby goes out with the mother and comes back. The "
       "record stays active and the exit is counted. Discharge closes the "
       "record with its outcome.",
       "Methode kangourou : le bebe sort avec la mere et revient. Le dossier "
       "reste actif et la sortie est comptee. La sortie definitive clot le "
       "dossier avec son resultat."),
};

const Step EXIT_STEPS[] = {
    EXPLAIN(&ui_TempToggleBtn, &ui_ScreenMain,
            "Cuando apagas la ultima terapia activa, el equipo entiende que el "
            "bebe sale y te pregunta por que: con la madre (canguro) o alta "
            "definitiva.",
            "When you switch off the last active therapy, the device "
            "understands the baby is leaving and asks why: with the mother "
            "(kangaroo) or final discharge.",
            "Quand vous eteignez la derniere therapie active, l'appareil "
            "comprend que le bebe sort et demande pourquoi : avec la mere "
            "(kangourou) ou sortie definitive."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalWizardOpen,
       "Primero necesitamos un bebe de practica: enciende el control de "
       "temperatura y completa el asistente.",
       "First we need a practice baby: turn temperature control on and "
       "complete the assistant.",
       "Il nous faut d'abord un bebe d'exercice : allumez le controle de "
       "temperature et completez l'assistant."),
    FREE(&ui_ScreenMain, goalTrainingBabyAdmitted,
         "Selecciona a ZOE, introduce peso y dias de vida y pulsa APLICAR.",
         "Select ZOE, enter weight and days of life and press APPLY.",
         "Selectionnez ZOE, saisissez poids et jours de vie et APPLIQUER."),
    // Paso libre y no "hacer" sobre el toggle: el dialogo de salida cuelga de
    // ui_ScreenMain, por debajo del overlay, y con sombras no se podria tocar.
    FREE_ENTER(&ui_ScreenMain, goalExitDone, enterExitStep,
             "Apaga el control de temperatura. En el dialogo elige ALTA, "
             "indica el resultado y, si procede, la causa. En formacion no se "
             "guarda nada.",
             "Turn temperature control off. In the dialog choose DISCHARGE, "
             "give the outcome and, if applicable, the cause. In training "
             "nothing is saved.",
             "Eteignez le controle de temperature. Dans la boite choisissez "
             "SORTIE, indiquez le resultat et, si besoin, la cause. En "
             "formation rien n'est enregistre."),
    EXPLAIN(nullptr, &ui_ScreenMain,
            "El alta cierra el registro: dias de estancia, pesos, horas de "
            "terapia y resultado quedan en el historial y en el servidor. Un "
            "nuevo bebe empieza de cero.",
            "Discharge closes the record: stay days, weights, therapy hours "
            "and outcome go to the history and the server. A new baby starts "
            "from scratch.",
            "La sortie clot le dossier : jours de sejour, poids, heures de "
            "therapie et resultat vont dans l'historique et au serveur. Un "
            "nouveau bebe repart de zero."),
    QUIZ(&ui_ScreenMain, &QUIZ_EXIT,
         "Pregunta: que significa la salida CON LA MADRE?",
         "Question: what does exit WITH THE MOTHER mean?",
         "Question : que signifie la sortie AVEC LA MERE ?"),
};

// ---- E8: bloqueo de pantalla -------------------------------------------------------

const Quiz QUIZ_LOCK = {
    {T3("Deja de medir y de regular", "It stops measuring and regulating",
        "Il cesse de mesurer et de reguler"),
     T3("Solo ignora toques; todo sigue funcionando y las alarmas se ven",
        "It only ignores touches; everything keeps working and alarms show",
        "Il ignore seulement les touches ; tout fonctionne et les alarmes "
        "s'affichent"),
     T3("Apaga la pantalla del todo", "It switches the screen fully off",
        "Il eteint completement l'ecran")},
    1,
    T3("El bloqueo evita toques accidentales, nada mas. La incubadora sigue "
       "regulando, la pantalla muestra los valores y las alarmas se ven y "
       "suenan igual.",
       "Locking prevents accidental touches, nothing more. The incubator "
       "keeps regulating, the screen shows the values and alarms show and "
       "sound as usual.",
       "Le verrouillage evite les touches accidentelles, rien de plus. "
       "L'incubateur continue de reguler, l'ecran montre les valeurs et les "
       "alarmes s'affichent et sonnent."),
};

const Step LOCK_STEPS[] = {
    EXPLAIN(&ui_ImgButton1, &ui_ScreenMain,
            "El candado bloquea la pantalla para que un roce no cambie nada. "
            "Tras 20 segundos sin tocar se bloquea sola; el anillo muestra el "
            "tiempo que falta.",
            "The padlock locks the screen so a brush does not change "
            "anything. After 20 seconds without touching it locks by itself; "
            "the ring shows the time left.",
            "Le cadenas verrouille l'ecran pour qu'un frolement ne change "
            "rien. Apres 20 secondes sans contact il se verrouille seul ; "
            "l'anneau montre le temps restant."),
    DO(&ui_ImgButton1, &ui_ScreenMain, goalLocked,
       "Toca el candado para bloquear.", "Touch the padlock to lock.",
       "Touchez le cadenas pour verrouiller."),
    EXPLAIN(nullptr, &ui_ScreenLock,
            "Pantalla bloqueada: siguen visibles temperaturas, humedad, "
            "alarmas y tendencia. Los toques no cambian nada.",
            "Locked screen: temperatures, humidity, alarms and trend stay "
            "visible. Touches change nothing.",
            "Ecran verrouille : temperatures, humidite, alarmes et tendance "
            "restent visibles. Les touches ne changent rien."),
    DO(&ui_ScreenLock, &ui_ScreenLock, goalUnlockPopup,
       "Toca en cualquier punto de la pantalla: aparece el boton de "
       "desbloqueo.",
       "Touch anywhere on the screen: the unlock button appears.",
       "Touchez n'importe ou sur l'ecran : le bouton de deverrouillage "
       "apparait."),
    DO(&ui_UnlockCont, &ui_ScreenLock, goalUnlocked,
       "Manten pulsado el boton de desbloqueo hasta que se complete el "
       "circulo (1,5 segundos).",
       "Hold the unlock button until the circle completes (1.5 seconds).",
       "Maintenez le bouton de deverrouillage jusqu'a ce que le cercle se "
       "complete (1,5 seconde)."),
    QUIZ(&ui_ScreenMain, &QUIZ_LOCK,
         "Pregunta: que hace la incubadora mientras la pantalla esta "
         "bloqueada?",
         "Question: what does the incubator do while the screen is locked?",
         "Question : que fait l'incubateur pendant que l'ecran est "
         "verrouille ?"),
};

// ---- E9: tendencia -----------------------------------------------------------------

const Quiz QUIZ_TREND = {
    {T3("Solo el ultimo valor medido", "Only the last measured value",
        "Seulement la derniere valeur mesuree"),
     T3("La evolucion de temperatura y humedad en las ultimas horas",
        "How temperature and humidity evolved over the last hours",
        "L'evolution de la temperature et de l'humidite ces dernieres "
        "heures"),
     T3("La lista de alarmas", "The alarm list", "La liste des alarmes")},
    1,
    T3("La tendencia dibuja aire, piel y humedad de los ultimos minutos u "
       "horas (5 min a 2 h). Sirve para ver si el equipo mantiene la "
       "consigna o hay oscilaciones.",
       "The trend draws air, skin and humidity over the last minutes or "
       "hours (5 min to 2 h). Use it to see whether the device holds the "
       "setpoint or oscillates.",
       "La tendance trace air, peau et humidite des dernieres minutes ou "
       "heures (5 min a 2 h). Elle montre si l'appareil tient la consigne ou "
       "oscille."),
};

const Step TREND_STEPS[] = {
    EXPLAIN(&ui_ImgButton1, &ui_ScreenMain,
            "La grafica de tendencia esta en la pantalla de bloqueo, para "
            "consultarla sin riesgo de tocar nada.",
            "The trend chart is on the lock screen, to consult it with no "
            "risk of touching anything.",
            "Le graphique de tendance est sur l'ecran verrouille, pour le "
            "consulter sans risque de toucher quoi que ce soit."),
    DO(&ui_ImgButton1, &ui_ScreenMain, goalLocked,
       "Bloquea la pantalla con el candado.", "Lock the screen with the padlock.",
       "Verrouillez l'ecran avec le cadenas."),
    DO(&ui_ChartLockImg, &ui_ScreenLock, goalTrendOpen,
       "Toca el icono de la grafica.", "Touch the chart icon.",
       "Touchez l'icone du graphique."),
    FREE(&ui_ScreenLock, goalTrendClosed,
         "Cambia la ventana de tiempo (5 min, 30 min, 1 h, 2 h) y mira las "
         "tres curvas. Cierra con la X para continuar.",
         "Change the time window (5 min, 30 min, 1 h, 2 h) and look at the "
         "three curves. Close with the X to continue.",
         "Changez la fenetre de temps (5 min, 30 min, 1 h, 2 h) et regardez "
         "les trois courbes. Fermez avec la X pour continuer."),
    DO(&ui_ScreenLock, &ui_ScreenLock, goalUnlockPopup,
       "Toca la pantalla para que aparezca el boton de desbloqueo.",
       "Touch the screen so the unlock button appears.",
       "Touchez l'ecran pour faire apparaitre le bouton de deverrouillage."),
    DO(&ui_UnlockCont, &ui_ScreenLock, goalUnlocked,
       "Manten pulsado para desbloquear.", "Hold to unlock.",
       "Maintenez pour deverrouiller."),
    QUIZ(&ui_ScreenMain, &QUIZ_TREND,
         "Pregunta: que muestra la grafica de tendencia?",
         "Question: what does the trend chart show?",
         "Question : que montre le graphique de tendance ?"),
};

// ---- E10: ajustar la hora ----------------------------------------------------------

const Quiz QUIZ_TIME = {
    {T3("Solo para mostrarla en el heading", "Only to show it in the heading",
        "Seulement pour l'afficher dans l'en-tete"),
     T3("Porque marca la hora de pesos, altas y alarmas en el historial",
        "Because it timestamps weights, discharges and alarms in the history",
        "Parce qu'elle date poids, sorties et alarmes dans l'historique"),
     T3("Para conectarse al servidor", "To connect to the server",
        "Pour se connecter au serveur")},
    1,
    T3("Todo lo que se registra (pesos, salidas, alarmas) lleva la hora del "
       "equipo. Con la hora mal, el historial clinico queda mal fechado.",
       "Everything recorded (weights, exits, alarms) carries the device time. "
       "With a wrong time, the clinical history is misdated.",
       "Tout ce qui est enregistre (poids, sorties, alarmes) porte l'heure de "
       "l'appareil. Avec une heure fausse, l'historique clinique est mal "
       "date."),
};

const Step TIME_STEPS[] = {
    DO(&ui_ClockButton, &ui_ScreenMain, goalTimeOpen,
       "Toca la hora del heading para ajustarla.",
       "Touch the time in the heading to set it.",
       "Touchez l'heure de l'en-tete pour la regler."),
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
            "Si el equipo tiene red, la hora se sincroniza sola. Sin red, "
            "revisala al encender: de ella dependen las fechas del historial.",
            "With a network the time syncs by itself. Without one, check it "
            "at power-on: the history dates depend on it.",
            "Avec un reseau l'heure se synchronise seule. Sans reseau, "
            "verifiez-la a l'allumage : les dates de l'historique en "
            "dependent."),
    QUIZ(&ui_ScreenMain, &QUIZ_TIME,
         "Pregunta: por que importa que la hora sea correcta?",
         "Question: why does a correct time matter?",
         "Question : pourquoi l'heure correcte est-elle importante ?"),
};

// ---- E11: contactar con soporte ------------------------------------------------------

const Step SUPPORT_STEPS[] = {
    DO(&ui_HelpButton, &ui_ScreenMain, goalHelpOpen,
       "Toca el boton de ayuda.", "Touch the help button.",
       "Touchez le bouton d'aide."),
    FREE(&ui_ScreenMain, goalHelpClosed,
         "Elige CONTACTAR SOPORTE. El QR abre un correo con el numero de "
         "serie y el estado del equipo ya rellenos: solo escribes tu consulta. "
         "Cierra con la X para continuar.",
         "Choose CONTACT SUPPORT. The QR opens an email with the serial "
         "number and device status already filled in: you just type your "
         "question. Close with the X to continue.",
         "Choisissez CONTACTER LE SUPPORT. Le QR ouvre un e-mail avec le "
         "numero de serie et l'etat de l'appareil deja remplis. Fermez avec "
         "la X pour continuer."),
    EXPLAIN(&ui_HelpButton, &ui_ScreenMain,
            "En el mismo menu tienes el video tutorial (QR a la web) y estos "
            "cursos. Fin del curso de enfermeria.",
            "The same menu has the video tutorial (QR to the website) and "
            "these courses. End of the nursing course.",
            "Le meme menu propose le tutoriel video (QR vers le site) et ces "
            "cours. Fin du cours de soins infirmiers."),
};

#define LESSON(id, es, en, fr, steps)                                          \
  { id, T3(es, en, fr), steps, (uint8_t)(sizeof(steps) / sizeof(steps[0])), \
    LESSON_INTERACTIVE, nullptr }
// Igual, pero solo se lista si `avail()` es verdadero (opcion de Ajustes).
#define LESSON_IF(id, es, en, fr, steps, avail)                                \
  { id, T3(es, en, fr), steps, (uint8_t)(sizeof(steps) / sizeof(steps[0])), \
    LESSON_INTERACTIVE, avail }

const Lesson NURSE_LESSONS[] = {
    LESSON_INTRO,
    LESSON(1, "Registrar y seguir a un bebe", "Registering and following a baby",
           "Enregistrer et suivre un bebe", BABY_STEPS),
    LESSON(2, "Temperatura por aire", "Air temperature", "Temperature par air",
           AIR_STEPS),
    LESSON_IF(3, "Control por piel y sonda", "Skin control and probe",
              "Controle peau et sonde", SKIN_STEPS, availSkin),
    LESSON_IF(4, "Humedad", "Humidity", "Humidite", HUM_STEPS, availHum),
    LESSON(5, "Fototerapia segura", "Safe phototherapy",
           "Phototherapie sure", PHOTO_STEPS),
    LESSON(6, "Atender una alarma", "Handling an alarm", "Gerer une alarme",
           ALARM_STEPS),
    LESSON(7, "Salida del bebe", "Baby exit", "Sortie du bebe", EXIT_STEPS),
    LESSON(8, "Bloqueo de pantalla", "Screen lock", "Verrouillage de l'ecran",
           LOCK_STEPS),
    LESSON(9, "Tendencia", "Trend", "Tendance", TREND_STEPS),
    LESSON(10, "Ajustar la hora", "Setting the time", "Regler l'heure",
           TIME_STEPS),
    LESSON(11, "Contactar con soporte", "Contacting support",
           "Contacter le support", SUPPORT_STEPS),
};

}  // namespace

const Course COURSE_NURSE = {
    TRAINING_COURSE_NURSE,
    T3("Enfermeria", "Nursing", "Soins infirmiers"),
    T3("Uso clinico: bebes, temperatura, humedad, fototerapia y alarmas.",
       "Clinical use: babies, temperature, humidity, phototherapy and alarms.",
       "Usage clinique : bebes, temperature, humidite, phototherapie et alarmes."),
    NURSE_LESSONS,
    (uint8_t)(sizeof(NURSE_LESSONS) / sizeof(NURSE_LESSONS[0])),
};
