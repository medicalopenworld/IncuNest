// Curso de Enfermeria (spec hmi-training-courses). E0 intro (compartida), E1
// temperatura por aire, E2 piel y sonda, E3 humedad, E4 fototerapia, E5
// atender una alarma, E6 alta y seguimiento, E7 salida del bebe, E8 bloqueo,
// E9 tendencia, E10 hora, E11 soporte.
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

// Temperatura
bool goalWizardOpen() { return BabyWizard_IsOpen(); }
bool goalTempOn() { return snap().switchTemp && !BabyWizard_IsOpen(); }
bool goalTempOff() { return !snap().switchTemp && !BabyWizard_IsOpen(); }
double s_airBase = 0.0;
void enterAirBase() { s_airBase = snap().airTempValue; }
bool goalAirUpTwo() { return snap().airTempValue >= s_airBase + 0.39; }

// Piel. La sonda es real (la placa la detecta): sin sonda el paso de activar
// el modo piel se da por cumplido para que la leccion no se quede colgada.
bool probeOk() { return g_skinProbeState == SKIN_PROBE_VALID; }
bool goalSettingsScreen() { return lv_scr_act() == ui_ScreenSettings; }
bool goalMainScreen() { return lv_scr_act() == ui_ScreenMain; }
bool goalModesVisible() { return visible(ui_ModesConfigCont); }
bool goalSkinEnabledOrNoProbe() { return snap().skinPanelEnabled || !probeOk(); }
bool goalSkinSelectedOrNoProbe() {
  return snap().selectedPanel == SKIN_PANEL_SELECTED || !probeOk() ||
         !snap().skinPanelEnabled;
}

// Humedad
bool goalHumOnOrWizard() { return snap().switchHum || BabyWizard_IsOpen(); }
bool goalHumOn() { return snap().switchHum && !BabyWizard_IsOpen(); }
bool goalHumOff() { return !snap().switchHum; }
int s_humBase = 0;
void enterHumBase() { s_humBase = snap().humValue; }
bool goalHumUp() { return snap().humValue >= s_humBase + 5; }

// Fototerapia
int s_photoMinBase = 0;
void enterPhotoBase() { s_photoMinBase = snap().photoTimerMinutes; }
bool goalPhotoMinUp() { return snap().photoTimerMinutes > s_photoMinBase; }
bool goalPhotoOnOrDialog() {
  return hmi_msg.phototherapyMode == PHOTOTHERAPY_ON || BabyWizard_IsOpen() ||
         visible(ui_PhotoSafetyOverlay);
}
bool goalPhotoOn() {
  return hmi_msg.phototherapyMode == PHOTOTHERAPY_ON && !BabyWizard_IsOpen() &&
         !visible(ui_PhotoSafetyOverlay);
}
bool goalPhotoTimerRunning() { return snap().photoTimerActive; }
bool goalPhotoTimerStopped() { return !snap().photoTimerActive; }
bool goalPhotoOff() { return hmi_msg.phototherapyMode == PHOTOTHERAPY_OFF; }

// Alarmas
bool goalAlarmCenterOpen() { return AlarmCenter_IsOpen(); }
bool goalAlarmCenterClosed() { return !AlarmCenter_IsOpen(); }

// Bebes
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

// Bloqueo
bool goalLocked() { return UI_IsScreenLocked(); }
bool goalUnlockPopup() { return visible(ui_UnlockCont); }
bool goalUnlocked() { return !UI_IsScreenLocked(); }

// Tendencia, hora, soporte
bool goalTrendOpen() { return TelemetryHistory_IsOpen(); }
bool goalTrendClosed() { return !TelemetryHistory_IsOpen(); }
bool goalTimeOpen() { return TimeDialog_IsOpen(); }
bool goalTimeClosed() { return !TimeDialog_IsOpen(); }
bool goalHelpOpen() { return HelpDialog_IsOpen(); }
bool goalHelpClosed() { return !HelpDialog_IsOpen(); }

// ---- E1: temperatura por aire ----------------------------------------------

const Quiz QUIZ_E1 = {
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

const Step E1_STEPS[] = {
    EXPLAIN(&ui_TempCont, &ui_ScreenMain,
            "Vamos a encender el control de temperatura por aire. La "
            "incubadora esta en modo formacion: lo que hagas no llega a la "
            "placa. Las medidas que ves si son reales.",
            "Let's turn on air temperature control. The incubator is in "
            "training mode: nothing you do reaches the board. The readings "
            "you see are real.",
            "Allumons le controle de temperature par air. L'incubateur est en "
            "mode formation : rien n'atteint la carte. Les mesures affichees "
            "sont reelles."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalWizardOpen,
       "Toca el boton de encendido del control de temperatura. Se abrira el "
       "asistente de datos del bebe.",
       "Touch the temperature control power button. The baby data assistant "
       "will open.",
       "Touchez le bouton de mise en marche du controle de temperature. "
       "L'assistant des donnees du bebe va s'ouvrir."),
    FREE(&ui_ScreenMain, goalTempOn,
         "Asistente del bebe: pulsa NUEVO BEBE y rellena nombre, semanas, "
         "peso y edad, o pulsa SALTAR para arrancar en manual. Al terminar, "
         "el control queda encendido.",
         "Baby assistant: press NEW BABY and fill in name, weeks, weight and "
         "age, or press SKIP to start in manual. When done, the control is "
         "on.",
         "Assistant bebe : appuyez sur NOUVEAU BEBE et remplissez nom, "
         "semaines, poids et age, ou SAUTER pour demarrer en manuel. A la "
         "fin, le controle est allume."),
    EXPLAIN(&ui_AirPanel, &ui_ScreenMain,
            "El control ha quedado en AIRE: regula por la temperatura de la "
            "cabina. PIEL usa la sonda sobre el bebe y solo esta disponible "
            "con la sonda conectada.",
            "Control is now in AIR: it regulates on cabin temperature. SKIN "
            "uses the probe on the baby and is only available with the probe "
            "connected.",
            "Le controle est en AIR : il regule sur la temperature de "
            "l'habitacle. PEAU utilise la sonde sur le bebe et n'est "
            "disponible que sonde branchee."),
    DO_ENTER(&ui_ImgArrowUpTemp, &ui_ScreenMain, goalAirUpTwo, enterAirBase,
             "Sube la consigna dos pasos con la flecha de arriba (0,2 grados "
             "cada toque).",
             "Raise the setpoint two steps with the up arrow (0.2 degrees per "
             "touch).",
             "Montez la consigne de deux pas avec la fleche haut (0,2 degre "
             "par appui)."),
    EXPLAIN(&ui_AirPanel, &ui_ScreenMain,
            "La cifra grande es la temperatura medida ahora; la pequena junto "
            "a las flechas es tu consigna. La incubadora calienta hasta "
            "igualarlas. Aqui nada cambia porque estamos en formacion.",
            "The big figure is the temperature measured now; the small one "
            "next to the arrows is your setpoint. The incubator heats until "
            "they match. Nothing changes here because we are in training.",
            "Le grand chiffre est la temperature mesuree ; le petit pres des "
            "fleches est votre consigne. L'incubateur chauffe jusqu'a les "
            "egaler. Ici rien ne change : nous sommes en formation."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalTempOff,
       "Apaga el control de temperatura con el mismo boton.",
       "Turn temperature control off with the same button.",
       "Eteignez le controle de temperature avec le meme bouton."),
    QUIZ(&ui_ScreenMain, &QUIZ_E1,
         "Pregunta: en el panel AIRE, la cifra grande es...",
         "Question: in the AIR panel, the big figure is...",
         "Question : dans le panneau AIR, le grand chiffre est..."),
};

// ---- E2: control por piel y sonda ----------------------------------------------

const Quiz QUIZ_E2 = {
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

const Step E2_STEPS[] = {
    EXPLAIN(&ui_TempCont, &ui_ScreenMain,
            "El modo PIEL regula por la temperatura del bebe, medida con la "
            "sonda pegada a su piel. Antes hay que habilitarlo en Ajustes y "
            "tener la sonda conectada.",
            "SKIN mode regulates on the baby's temperature, measured by the "
            "probe stuck to the skin. First enable it in Settings and have the "
            "probe connected.",
            "Le mode PEAU regule sur la temperature du bebe, mesuree par la "
            "sonde collee a la peau. Activez-le d'abord dans Reglages, sonde "
            "branchee."),
    DO(&ui_Settings, &ui_ScreenMain, goalSettingsScreen,
       "Toca Ajustes.", "Touch Settings.", "Touchez Reglages."),
    DO(&ui_ModesButton, &ui_ScreenSettings, goalModesVisible,
       "Toca la fila Modos.", "Touch the Modes row.", "Touchez la ligne Modes."),
    DO(&ui_Switch4, &ui_ScreenSettings, goalSkinEnabledOrNoProbe,
       "Activa el interruptor de modo piel. Si la sonda no esta conectada, el "
       "equipo lo rechaza con un aviso y este paso se salta.",
       "Turn on the skin mode switch. If the probe is not connected, the "
       "device refuses with a warning and this step is skipped.",
       "Activez l'interrupteur mode peau. Sans sonde branchee, l'appareil "
       "refuse avec un avertissement et cette etape est sautee."),
    DO(&ui_ImgButton2, &ui_ScreenSettings, goalMainScreen,
       "Vuelve a la pantalla principal con la flecha.",
       "Go back to the main screen with the arrow.",
       "Revenez a l'ecran principal avec la fleche."),
    DO(&ui_TempToggleBtn, &ui_ScreenMain, goalWizardOpen,
       "Enciende el control de temperatura.",
       "Turn temperature control on.",
       "Allumez le controle de temperature."),
    FREE(&ui_ScreenMain, goalTempOn,
         "Completa el asistente del bebe o pulsa SALTAR.",
         "Complete the baby assistant or press SKIP.",
         "Completez l'assistant bebe ou appuyez sur SAUTER."),
    DO(&ui_SkinPanel, &ui_ScreenMain, goalSkinSelectedOrNoProbe,
       "Toca PIEL para regular por la temperatura del bebe. Sin sonda este "
       "paso se salta.",
       "Touch SKIN to regulate on the baby's temperature. Without a probe "
       "this step is skipped.",
       "Touchez PEAU pour reguler sur la temperature du bebe. Sans sonde "
       "cette etape est sautee."),
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
    QUIZ(&ui_ScreenMain, &QUIZ_E2,
         "Pregunta: que hace falta para usar el modo PIEL?",
         "Question: what is needed to use SKIN mode?",
         "Question : que faut-il pour utiliser le mode PEAU ?"),
};

// ---- E3: humedad ---------------------------------------------------------------

const Quiz QUIZ_E3 = {
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

const Step E3_STEPS[] = {
    EXPLAIN(&ui_HumCont, &ui_ScreenMain,
            "Control de humedad. La cifra grande es la humedad medida en la "
            "cabina; la pequena, tu consigna. Si no ves este panel, esta "
            "oculto en Ajustes > Modos.",
            "Humidity control. The big figure is the humidity measured in the "
            "cabin; the small one, your setpoint. If you do not see this "
            "panel, it is hidden in Settings > Modes.",
            "Controle d'humidite. Le grand chiffre est l'humidite mesuree ; "
            "le petit, votre consigne. Si ce panneau n'apparait pas, il est "
            "masque dans Reglages > Modes."),
    DO(&ui_HumToggleBtn, &ui_ScreenMain, goalHumOnOrWizard,
       "Toca el boton de encendido de la humedad. Si no hay bebe "
       "identificado, se abrira el asistente.",
       "Touch the humidity power button. If no baby is identified, the "
       "assistant will open.",
       "Touchez le bouton de mise en marche de l'humidite. Sans bebe "
       "identifie, l'assistant s'ouvre."),
    FREE(&ui_ScreenMain, goalHumOn,
         "Completa el asistente del bebe (nombre y semanas) o pulsa SALTAR. "
         "Al terminar, la humedad queda encendida.",
         "Complete the baby assistant (name and weeks) or press SKIP. When "
         "done, humidity is on.",
         "Completez l'assistant bebe (nom et semaines) ou SAUTER. A la fin, "
         "l'humidite est allumee."),
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
    QUIZ(&ui_ScreenMain, &QUIZ_E3,
         "Pregunta: para que sirve la humedad en la incubadora?",
         "Question: what is humidity for in the incubator?",
         "Question : a quoi sert l'humidite dans l'incubateur ?"),
};

// ---- E4: fototerapia segura -----------------------------------------------------

const Quiz QUIZ_E4 = {
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

const Step E4_STEPS[] = {
    EXPLAIN(&ui_PhotoCont, &ui_ScreenMain,
            "Fototerapia. Aqui fijas los minutos de tratamiento y la "
            "enciendes. Recuerda: la luz es perjudicial para los ojos, hay "
            "que cubrirlos siempre antes.",
            "Phototherapy. Here you set the treatment minutes and switch it "
            "on. Remember: the light harms the eyes, always cover them first.",
            "Phototherapie. Ici vous fixez les minutes de traitement et "
            "l'allumez. Rappel : la lumiere abime les yeux, couvrez-les "
            "toujours avant."),
    DO(&ui_PhotoToggleBtn, &ui_ScreenMain, goalPhotoOnOrDialog,
       "Toca el boton de encendido de la fototerapia.",
       "Touch the phototherapy power button.",
       "Touchez le bouton de mise en marche de la phototherapie."),
    FREE(&ui_ScreenMain, goalPhotoOn,
         "Completa el asistente del bebe o pulsa SALTAR. Despues, en el aviso "
         "de proteccion ocular, confirma que los ojos estan cubiertos.",
         "Complete the baby assistant or press SKIP. Then, in the eye "
         "protection notice, confirm the eyes are covered.",
         "Completez l'assistant bebe ou SAUTER. Puis, dans l'avis de "
         "protection oculaire, confirmez que les yeux sont couverts."),
    DO_ENTER(&ui_PhotoTimePlusBtn, &ui_ScreenMain, goalPhotoMinUp,
             enterPhotoBase,
             "Sube los minutos del temporizador con el +.",
             "Raise the timer minutes with +.",
             "Augmentez les minutes du minuteur avec +."),
    DO(&ui_PhotoStartBtn, &ui_ScreenMain, goalPhotoTimerRunning,
       "Pulsa INICIAR: arranca la cuenta atras y la lampara se apagara sola "
       "al terminar.",
       "Press START: the countdown begins and the lamp will switch off by "
       "itself at the end.",
       "Appuyez sur DEMARRER : le compte a rebours commence et la lampe "
       "s'eteindra seule a la fin."),
    DO(&ui_PhotoCancelBtn, &ui_ScreenMain, goalPhotoTimerStopped,
       "Cancela la cuenta atras. Sin temporizador la lampara sigue encendida "
       "hasta que la apagues tu.",
       "Cancel the countdown. Without a timer the lamp stays on until you "
       "switch it off.",
       "Annulez le compte a rebours. Sans minuteur la lampe reste allumee "
       "jusqu'a ce que vous l'eteigniez."),
    DO(&ui_PhotoToggleBtn, &ui_ScreenMain, goalPhotoOff,
       "Apaga la fototerapia.", "Turn phototherapy off.",
       "Eteignez la phototherapie."),
    QUIZ(&ui_ScreenMain, &QUIZ_E4,
         "Pregunta: que hay que hacer SIEMPRE antes de encender la "
         "fototerapia?",
         "Question: what must ALWAYS be done before switching phototherapy "
         "on?",
         "Question : que faut-il TOUJOURS faire avant d'allumer la "
         "phototherapie ?"),
};

// ---- E5: atender una alarma -------------------------------------------------

const Quiz QUIZ_E5 = {
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

const Step E5_STEPS[] = {
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
    QUIZ(&ui_ScreenMain, &QUIZ_E5,
         "Pregunta: que hace el boton de pausa de audio?",
         "Question: what does the audio pause button do?",
         "Question : que fait le bouton de pause audio ?"),
};

// ---- E6: alta y seguimiento del bebe -------------------------------------------

const Quiz QUIZ_E6 = {
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

const Step E6_STEPS[] = {
    EXPLAIN(&ui_BabiesButton, &ui_ScreenMain,
            "Cada bebe tiene su registro: nombre, semanas, pesos, horas de "
            "terapia y salidas. Se crea al encender una terapia por primera "
            "vez, con el asistente.",
            "Each baby has a record: name, weeks, weights, therapy hours and "
            "exits. It is created when a therapy is first switched on, with "
            "the assistant.",
            "Chaque bebe a son dossier : nom, semaines, poids, heures de "
            "therapie et sorties. Il se cree a la premiere mise en marche "
            "d'une therapie, avec l'assistant."),
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
       "Ahora vamos a dar de alta a un bebe de practica. Enciende el control "
       "de temperatura para abrir el asistente.",
       "Now let's admit a practice baby. Turn temperature control on to open "
       "the assistant.",
       "Admettons maintenant un bebe d'exercice. Allumez le controle de "
       "temperature pour ouvrir l'assistant."),
    FREE(&ui_ScreenMain, goalTrainingBabyAdmitted,
         "Pulsa NUEVO BEBE. Rellena nombre, semanas de gestacion, peso en "
         "gramos y dias de vida. Lee la temperatura propuesta y pulsa "
         "APLICAR. En formacion no se guarda nada.",
         "Press NEW BABY. Fill in name, gestational weeks, weight in grams and "
         "days of life. Read the proposed temperature and press APPLY. In "
         "training nothing is saved.",
         "Appuyez sur NOUVEAU BEBE. Remplissez nom, semaines de gestation, "
         "poids en grammes et jours de vie. Lisez la temperature proposee et "
         "APPLIQUER. En formation rien n'est enregistre."),
    EXPLAIN(&ui_AirPanel, &ui_ScreenMain,
            "La consigna propuesta sale del rango de temperatura neutra para "
            "ese peso y edad. Puedes ajustarla con las flechas si el medico "
            "indica otra.",
            "The proposed setpoint comes from the neutral temperature range "
            "for that weight and age. You can adjust it with the arrows if the "
            "doctor indicates otherwise.",
            "La consigne proposee vient de la plage de temperature neutre "
            "pour ce poids et cet age. Ajustez-la avec les fleches si le "
            "medecin l'indique."),
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
    QUIZ(&ui_ScreenMain, &QUIZ_E6,
         "Pregunta: para que pide el equipo los datos del bebe?",
         "Question: why does the device ask for the baby data?",
         "Question : pourquoi l'appareil demande-t-il les donnees du bebe ?"),
};

// ---- E7: salida del bebe ---------------------------------------------------------

const Quiz QUIZ_E7 = {
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

const Step E7_STEPS[] = {
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
         "NUEVO BEBE, nombre, semanas, peso, edad, APLICAR.",
         "NEW BABY, name, weeks, weight, age, APPLY.",
         "NOUVEAU BEBE, nom, semaines, poids, age, APPLIQUER."),
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
    QUIZ(&ui_ScreenMain, &QUIZ_E7,
         "Pregunta: que significa la salida CON LA MADRE?",
         "Question: what does exit WITH THE MOTHER mean?",
         "Question : que signifie la sortie AVEC LA MERE ?"),
};

// ---- E8: bloqueo de pantalla -------------------------------------------------------

const Quiz QUIZ_E8 = {
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

const Step E8_STEPS[] = {
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
    QUIZ(&ui_ScreenMain, &QUIZ_E8,
         "Pregunta: que hace la incubadora mientras la pantalla esta "
         "bloqueada?",
         "Question: what does the incubator do while the screen is locked?",
         "Question : que fait l'incubateur pendant que l'ecran est "
         "verrouille ?"),
};

// ---- E9: tendencia -----------------------------------------------------------------

const Quiz QUIZ_E9 = {
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

const Step E9_STEPS[] = {
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
    QUIZ(&ui_ScreenMain, &QUIZ_E9,
         "Pregunta: que muestra la grafica de tendencia?",
         "Question: what does the trend chart show?",
         "Question : que montre le graphique de tendance ?"),
};

// ---- E10: ajustar la hora ----------------------------------------------------------

const Quiz QUIZ_E10 = {
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

const Step E10_STEPS[] = {
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
    QUIZ(&ui_ScreenMain, &QUIZ_E10,
         "Pregunta: por que importa que la hora sea correcta?",
         "Question: why does a correct time matter?",
         "Question : pourquoi l'heure correcte est-elle importante ?"),
};

// ---- E11: contactar con soporte ------------------------------------------------------

const Step E11_STEPS[] = {
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
    LESSON_INTERACTIVE }

const Lesson NURSE_LESSONS[] = {
    LESSON_INTRO,
    LESSON(1, "Temperatura por aire", "Air temperature", "Temperature par air",
           E1_STEPS),
    LESSON(2, "Control por piel y sonda", "Skin control and probe",
           "Controle peau et sonde", E2_STEPS),
    LESSON(3, "Humedad", "Humidity", "Humidite", E3_STEPS),
    LESSON(4, "Fototerapia segura", "Safe phototherapy",
           "Phototherapie sure", E4_STEPS),
    LESSON(5, "Atender una alarma", "Handling an alarm", "Gerer une alarme",
           E5_STEPS),
    LESSON(6, "Alta y seguimiento del bebe", "Admitting and following a baby",
           "Admission et suivi du bebe", E6_STEPS),
    LESSON(7, "Salida del bebe", "Baby exit", "Sortie du bebe", E7_STEPS),
    LESSON(8, "Bloqueo de pantalla", "Screen lock", "Verrouillage de l'ecran",
           E8_STEPS),
    LESSON(9, "Tendencia", "Trend", "Tendance", E9_STEPS),
    LESSON(10, "Ajustar la hora", "Setting the time", "Regler l'heure",
           E10_STEPS),
    LESSON(11, "Contactar con soporte", "Contacting support",
           "Contacter le support", E11_STEPS),
};

}  // namespace

const Course COURSE_NURSE = {
    TRAINING_COURSE_NURSE,
    T3("Enfermeria", "Nursing", "Soins infirmiers"),
    T3("Uso clinico: temperatura, humedad, fototerapia, alarmas y bebes.",
       "Clinical use: temperature, humidity, phototherapy, alarms and babies.",
       "Usage clinique : temperature, humidite, phototherapie, alarmes et bebes."),
    NURSE_LESSONS,
    (uint8_t)(sizeof(NURSE_LESSONS) / sizeof(NURSE_LESSONS[0])),
};
