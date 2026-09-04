// Curso de Enfermeria (spec hmi-training-courses). Fase 1: E0 intro, E1
// temperatura por aire, E5 atender una alarma. El resto de lecciones llega en
// la fase 2 (tasks.md, seccion 8).
#include "ui/training/lessons.h"

#include "UITask.h"
#include "main.h"
#include "ui.h"
#include "ui/AlarmCenter.h"
#include "ui/BabyWizard.h"

extern const Lesson LESSON_INTRO;  // lessons_intro.cpp

namespace {

// ---- Objetivos (se evaluan por estado en cada vuelta de UI) -------------------

UiControlSnapshot snap() {
  UiControlSnapshot s;
  UI_GetControlSnapshot(&s);
  return s;
}

bool goalWizardOpen() { return BabyWizard_IsOpen(); }
bool goalTempOn() { return snap().switchTemp && !BabyWizard_IsOpen(); }
bool goalTempOff() { return !snap().switchTemp && !BabyWizard_IsOpen(); }

// Consigna de partida del paso "sube dos pasos": se captura al entrar.
double s_airBase = 0.0;
void enterAirBase() { s_airBase = snap().airTempValue; }
bool goalAirUpTwo() { return snap().airTempValue >= s_airBase + 0.39; }

bool goalAlarmCenterOpen() { return AlarmCenter_IsOpen(); }
bool goalAlarmCenterClosed() { return !AlarmCenter_IsOpen(); }

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
    // El asistente deja AIRE seleccionado (ActivateTempControlUI(true)), asi
    // que un paso "toca AIRE" se saltaria siempre por objetivo ya cumplido:
    // se explica en vez de pedirlo.
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
       "Touch the alarm icon to open the alarm center. Inside, each alarm "
       "shows its title and the recommended action; the pause button mutes "
       "the sound for a while. When done, close with the X.",
       "Touchez l'icone d'alarmes pour ouvrir le centre d'alarmes. Chaque "
       "alarme y montre son titre et l'action recommandee ; le bouton pause "
       "coupe le son un moment. Ensuite, fermez avec la X."),
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

const Lesson NURSE_LESSONS[] = {
    LESSON_INTRO,
    {1, T3("Temperatura por aire", "Air temperature", "Temperature par air"),
     E1_STEPS, (uint8_t)(sizeof(E1_STEPS) / sizeof(E1_STEPS[0])),
     LESSON_INTERACTIVE},
    {2, T3("Atender una alarma", "Handling an alarm", "Gerer une alarme"),
     E5_STEPS, (uint8_t)(sizeof(E5_STEPS) / sizeof(E5_STEPS[0])),
     LESSON_INTERACTIVE},
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
