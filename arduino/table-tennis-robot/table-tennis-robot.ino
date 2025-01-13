#include <AFMotor.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// Define as portas para SoftwareSerial
SoftwareSerial BTSerial(14, 15); // RX, TX

// Motores de lançamento
AF_DCMotor motorTopSpin(1);
AF_DCMotor motorBackSpin(2);

// Motor do reservatório
AF_DCMotor motorReservatorio(3);

// Servomotor da base
Servo servoBase;
int anguloAtual = 90; // Posição inicial do servo (meio)
int velocidadeOscilacao = 250; // Velocidade inicial da oscilação (ms por passo)
bool oscilando = false; // Estado da oscilação
int direcaoOscilacao = 1; // Direção inicial da oscilação (1: direita, -1: esquerda)
unsigned long ultimoMovimentoOscilacao = 0; // Marca o último movimento de oscilação

// Variáveis e estados iniciais dos motores
int velocidadeTopSpin = 0;
int velocidadeBackSpin = 0;
int velocidadeReservatorio = 0;
int estadoReservatorio = 0; // 0: desligado, 1: ligado
int stepVelocidade = 10;
int stepAngulacao = 2;

// Variável para receber os comandos via serial
char comando = 0;

// Sensor LM393 para medir RPM do motor do reservatório
#define SENSOR_PIN 2 // Pino digital 2 para interrupções
volatile unsigned int pulsos = 0; // Contador de pulsos
unsigned long ultimoTempo = 0; // Para cálculo de RPM
const int pulsosPorRevolucao = 20; // Número de pulsos por revolução (depende do encoder do motor)
float rpmAtual = 0;
float rpmDesejado = 0; // RPM desejado para o motor do reservatório
const unsigned long intervalo = 500; //intervalo para aferição - valores baixos interferem negativamente no loop

// Variáveis para o controle PID
const float Kp = 0.5;
const float Ki = 0.2;
const float Kd = 0.05;
float erroAnterior = 0;
float erro = 0;
float integral = 0;
float saidaPID = 0;

void setup() {
  // Inicializa a comunicação serial
  Serial.begin(9600);
  pararMotores(); // Garante que todos os motores estejam parados no início
  BTSerial.begin(38400);     // Comunicação com o módulo HC-05
  BTSerial.println("Pronto para receber comandos via bluetooth.");

  //configuração servo motor
  servoBase.attach(10); // Conecte o servo ao pino 10 - primeiros pinos (ser1) do shield L293D
  servoBase.write(anguloAtual); // Posiciona o servo no ângulo inicial

  // Configuração do sensor de RPM
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), contarPulso, RISING);  
}

void loop() {
  //Verificação se o reservatório. Necessário para que ele inicie desligado.  
  if (estadoReservatorio) {
    // Calcule o erro e ajuste a velocidade com o PID
    erro = rpmDesejado - rpmAtual;
    float controlOutput = (erro * Kp) + (integral * Ki) + ((erro - erroAnterior) * Kd);
    // Limitar o valor do controle para evitar valores fora do intervalo
    controlOutput = constrain(controlOutput, 20, 140);
    // Ajuste a velocidade do motor
    velocidadeReservatorio = ajustarVelocidade(controlOutput);
    motorReservatorio.setSpeed(velocidadeReservatorio);
    motorReservatorio.run(FORWARD);
  } else {
    // Pare o motor se o reservatório estiver desligado
    motorReservatorio.setSpeed(0);
    motorReservatorio.run(RELEASE);
  }
  // Atualize o erro anterior para o próximo ciclo
  erroAnterior = erro;
 
  // Verifica se há dados recebidos via bluetooth
  if (BTSerial.available() > 0) {
    comando = BTSerial.read();

    switch (comando) {
            
      case 'A': // Aumentar velocidade do motor topspin
        velocidadeTopSpin = ajustarVelocidade(velocidadeTopSpin + stepVelocidade);
        motorTopSpin.setSpeed(velocidadeTopSpin);
        motorTopSpin.run(BACKWARD);
        Serial.println("Velocidade topspin: " + String(velocidadeTopSpin));
        //envia o caracter N para o textbox do tpospin no bluetooth electronics
        //informando em seguida a velocidade a ser apresentada no textbox.
        BTSerial.println("*N"+String(velocidadeTopSpin)+"*");
        break;
        
      case 'B': // Diminuir velocidade do motor topspin
        velocidadeTopSpin = ajustarVelocidade(velocidadeTopSpin - stepVelocidade);
        motorTopSpin.setSpeed(velocidadeTopSpin);
        motorTopSpin.run(BACKWARD);
        Serial.println("Velocidade topspin: " + String(velocidadeTopSpin));
        BTSerial.println("*N"+String(velocidadeTopSpin)+"*");
        break;
        
      case 'C': // Aumentar velocidade do motor backspin
        velocidadeBackSpin = ajustarVelocidade(velocidadeBackSpin + stepVelocidade);
        motorBackSpin.setSpeed(velocidadeBackSpin);
        motorBackSpin.run(FORWARD);
        Serial.println("Velocidade backspin: " + String(velocidadeBackSpin));
        BTSerial.println("*O"+String(velocidadeBackSpin)+"*");
        break;
        
      case 'D': // Diminuir velocidade do motor backspin
        velocidadeBackSpin = ajustarVelocidade(velocidadeBackSpin - stepVelocidade);
        motorBackSpin.setSpeed(velocidadeBackSpin);
        motorBackSpin.run(FORWARD);
        Serial.println("Velocidade backspin: " + String(velocidadeBackSpin));
        BTSerial.println("*O"+String(velocidadeBackSpin)+"*");
        break;

      case 'E': // Liga/desliga o motor do reservatório
        if (estadoReservatorio == 0) {
          motorReservatorio.setSpeed(velocidadeReservatorio);
          motorReservatorio.run(FORWARD);
          estadoReservatorio = 1;
          Serial.println("Reservatório ligado.");
        } else {
          motorReservatorio.run(RELEASE);
          estadoReservatorio = 0;
          rpmAtual = 0; // Reseta o valor de RPM atual
          velocidadeReservatorio = 0; // Reseta a velocidade do motor
          rpmDesejado = 0;  //Reseta o valor de RPM desejado
          Serial.println("Reservatório desligado.");
        }
        break;
        
      case 'F': // Aumentar velocidade do reservatório
       rpmDesejado += stepVelocidade;
       Serial.println("RPM Atual: " + String(rpmAtual));
       Serial.println("RPM desejado: " + String(rpmDesejado));
       Serial.println("Velocidade ajustada: " + String(velocidadeReservatorio));
       BTSerial.println("*P"+String(rpmDesejado)+"*");
       break;
        
      case 'G': // Diminuir velocidade do reservatório
       rpmDesejado -= stepVelocidade;
        Serial.println("RPM Atual: " + String(rpmAtual));
        Serial.println("RPM desejado: " + String(rpmDesejado));
        Serial.println("Velocidade ajustada: " + String(velocidadeReservatorio));
        BTSerial.println("*P"+String(rpmDesejado)+"*");
        break;
      
      // Botão de emergência
      case 'H':
        pararMotores();
        Serial.println("Parada de emergência ativada.");
        break;

      // Controle do servo motor para movimento horizontal
      case 'I': // Girar base para a direita
        anguloAtual = ajustarAngulo(anguloAtual - stepAngulacao);
        servoBase.write(anguloAtual);
        Serial.println("Base girada para a direita. Ângulo: " + String(anguloAtual));
        BTSerial.println("*Q"+String(anguloAtual)+"*");
        break;
        
      case 'J': // Girar base para a esquerda
        anguloAtual = ajustarAngulo(anguloAtual + stepAngulacao);
        servoBase.write(anguloAtual);
        Serial.println("Base girada para a esquerda. Ângulo: " + String(anguloAtual));
        BTSerial.println("*Q"+String(anguloAtual)+"*");
        break;   

      // Controle da oscilação do servo da base
      case 'K': // Liga/desliga o servo motor da base
        oscilando = !oscilando;
        Serial.println(oscilando ? "Oscilação ativada." : "Oscilação desativada.");
        break;
        
      case 'L':
        velocidadeOscilacao = max(50, velocidadeOscilacao - 5); // Aumenta a velocidade (diminui o delay)
        Serial.println("Velocidade de oscilação aumentada: " + String(velocidadeOscilacao));
        BTSerial.println("*R"+String(velocidadeOscilacao)+"*");
        break;
        
      case 'M':
        velocidadeOscilacao = min(250, velocidadeOscilacao + 5); // Diminui a velocidade (aumenta o delay)
        Serial.println("Velocidade de oscilação diminuída: " + String(velocidadeOscilacao));
        BTSerial.println("*R"+String(velocidadeOscilacao)+"*");
        break;
/*
 Mapa de posições específicas para lançamento

            MESA
   |---------|------|      
   |       P1| P4 P7|
   |Robo   P2| P5 P8|
   |       P3| P6 P9|
   ----------|--------       
 */

      case '1': // Configurar para posição P1
        configurarPosicao(50, 50, 105);
        break;

      case '2': // Configurar para posição P2
        configurarPosicao(45, 45, 90);
        break;
 
      case '3': // Configurar para posição P3
        configurarPosicao(50, 50, 75);
        break;

      case '4': // Configurar para posição P4
        configurarPosicao(80, 80, 105);
        break;

      case '5': // Configurar para posição P5
        configurarPosicao(75, 75, 90);
        break;

      case '6': // Configurar para posição P6
        configurarPosicao(80, 80, 75);
        break;

      case '7': // Configurar para posição P7
        configurarPosicao(120, 120, 105);
        break;

      case '8': // Configurar para posição P8
        configurarPosicao(115, 115, 90);
        break;

      case '9': // Configurar para posição P9
        configurarPosicao(120, 120, 75);
        break;
    
      default:
        break;
    }
  }


  if (millis() - ultimoTempo >= intervalo) {
    calcularRPM();
    ajustarReservatorioPID();
    ultimoTempo = millis();
  }

  if (oscilando && millis() - ultimoMovimentoOscilacao >= velocidadeOscilacao) {
    anguloAtual += direcaoOscilacao * stepAngulacao;
    if (anguloAtual >= 105 || anguloAtual <= 75) {
      direcaoOscilacao *= -1; // Inverte a direção
      anguloAtual = ajustarAngulo(anguloAtual); // Garante que não ultrapasse os limites
    }
    servoBase.write(anguloAtual);
    ultimoMovimentoOscilacao = millis();
  }  
}

// Função de interrupção para contar os pulsos do sensor de velocidade encoder lm393
void contarPulso() {
  pulsos++;
}

// Função para calcular o RPM do motor do reservatório
void calcularRPM() {
  rpmAtual = (pulsos * 60000.0) / (pulsosPorRevolucao * (millis() - ultimoTempo));
  pulsos = 0; // Reseta os pulsos
}

// Função para ajustar a velocidade do motor do reservatório
void ajustarReservatorioPID() {
  erro = rpmDesejado - rpmAtual;
  integral += erro * (intervalo / 1000.0); // Termo integral
  float derivada = (erro - erroAnterior) / (intervalo / 1000.0); // Termo derivativo
  saidaPID = (Kp * erro) + (Ki * integral) + (Kd * derivada);
  erroAnterior = erro;
  // Ajusta a velocidade do motor com base na saída do PID
  velocidadeReservatorio += saidaPID;
  velocidadeReservatorio = ajustarVelocidade(velocidadeReservatorio);
  motorReservatorio.setSpeed(velocidadeReservatorio);
  motorReservatorio.run(FORWARD);
}

// Função para ajustar velocidade dos motores dentro do limite de 0 a 240
int ajustarVelocidade(int velocidade) {
  if (velocidade > 240) return 240;
  if (velocidade < 40) return 40;
  return velocidade;
}

// Função para ajustar ângulo do servo motor dentro do limite de 75 a 105
int ajustarAngulo(int angulo) {
  if (angulo > 105) return 105;
  if (angulo < 75) return 75;
  return angulo;
}


// Função para ajustar velocidade e angulo para posições específicas
void configurarPosicao(int topSpin, int backSpin, int angulo) {
  velocidadeTopSpin = ajustarVelocidade(topSpin);
  velocidadeBackSpin = ajustarVelocidade(backSpin);
  anguloAtual = ajustarAngulo(angulo);
  motorTopSpin.setSpeed(velocidadeTopSpin);
  motorTopSpin.run(BACKWARD);
  motorBackSpin.setSpeed(velocidadeBackSpin);
  motorBackSpin.run(FORWARD);
  servoBase.write(anguloAtual);

  Serial.println("Configuração de posição realizada: ");
  Serial.println("TopSpin: " + String(velocidadeTopSpin));
  Serial.println("BackSpin: " + String(velocidadeBackSpin));
  Serial.println("Ângulo: " + String(anguloAtual));
}

// Função para parar todos os motores
void pararMotores() {
  motorTopSpin.run(RELEASE);
  motorBackSpin.run(RELEASE);
  motorReservatorio.run(RELEASE);
  velocidadeTopSpin = 0;
  velocidadeBackSpin = 0;
  velocidadeReservatorio = 0;
  rpmDesejado = 0;
  estadoReservatorio = 0;
  anguloAtual = 90;
  oscilando = false;  
}
