#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10 // PINO SDA
#define RST_PIN 9 // PINO DE RESET

/**
 * Cada cartão RFID tem 16 setores (0 ao 15) Obs: Alguns podem ter mais, atente-se a isso.
 * Cada setor tem 4 blocos (setor 0 tem os blocos 0, 1, 2 e 3. setor 1 tem os blocos 4, 5, 6 e 7...)
 * O primeiro bloco do setor 0 é reservado para armazenar dados do fabricante (ou seja bloco 0 do setro 0).
 * O ultimo bloco de cada setor é reservado para armazenar a chave de acesso do setor (bloco 3 do setor 0, bloco 7 do setor 1 etc...)
 * Ou seja o setor 0 tem apenas 2 blocos para escrita, ja que o bloco 0 e 3 estão reservados.
 * Os demais blocos 1, 2 ... 14, 15 tem 3 blocos para escrita.
*/

// Número do bloco para escrita
int blockNumber = 1;

// Dados para escrita (16 caracteres por bloco), no array contém o número 17 para evitar o erro de caractere, mas a string deve ter 16!
byte blockData[17] = { "@GuilhermeAw.com" };

// O tamanho do array de leitura precisa ser 2 Bytes maior que os 16 armazenados pelo bloco.
byte bufferLen = 18;

// Array vazio para armazenar os dados lidos do bloco.
byte readBlockData[18];

// Passagem de parâmetro referente aos pinos conectados no arduino.
MFRC522 rfid(SS_PIN, RST_PIN);

// Crio uma instância do MIFARE_Key para informar a key A
MFRC522::MIFARE_Key keyA;

// Crio uma instância do MIFARE_Key para informar a key B
MFRC522::MIFARE_Key keyB;

// Crio uma instância do StatusCode verificar a gravação e leitura
MFRC522::StatusCode status;

void SetKeys();
void DumpInfo();
void ReadRFID_UID();
bool IgnoreReservedBlocks(int AtualBlock);
void WriteDataToBlock(int blockNumber, byte blockData[]);
void ReadDataFromBlock(int blockNumber, byte readBlockData[]);

void setup() {
    Serial.begin(9600);
    SPI.begin();
    rfid.PCD_Init();
    SetKeys();
}

void loop() {

    // Verifica se o cartão está presente no leitor e se ele pode ser lido
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return;
    }

    ReadRFID_UID();

    // DumpInfo();
    // WriteDataToBlock(blockNumber, blockData);
    // ReadDataFromBlock(blockNumber, readBlockData);

    // Loop que percorrerá todos os blocos
    for (int i = 0; i <= 63; i++) {
        blockNumber = i;
        // Verifica se o bloco atual não é reservado pelo sistema.
        if(!IgnoreReservedBlocks(blockNumber)) {
            WriteDataToBlock(blockNumber, blockData);
            ReadDataFromBlock(blockNumber, readBlockData);
        }
    }

}

// Criando um array com as chaves de acesso padrão de fábrica (A e B) para executar as autenticações.
void SetKeys() {
    for (byte i = 0; i < 6; i++) {
        keyA.keyByte[i] = 0xFF;
        keyB.keyByte[i] = 0xFF;
    }
}

// Ignora os blocos que contém as chaves de acesso e as informações do fabricante.
bool IgnoreReservedBlocks(int AtualBlock) {
    bool found = false;
    // Blocos reservados
    int array[] = {0, 3, 7, 11, 15, 19, 23, 27, 31, 35, 39, 43, 47, 51, 55, 59, 63};
    for (int i = 0; i < 17; i++) {
        if(array[i] == AtualBlock) {
            found = true;
            break;
        }
    }
    return found;
}

// Executa uma varredura em todo a TAG e mostra todos os dados na serial
void DumpInfo() {
    rfid.PCD_DumpVersionToSerial();
    rfid.PICC_DumpToSerial(&(rfid.uid));
    return;
}

// Retorna algumas informações importantes da TAG, como UID, Dados do Fabricante etc..
void ReadRFID_UID() {

    // Inicio o bloco de código responsável por capturar o UID da TAG
    String strID = "";
    for (byte i = 0; i < 4; i++) {
        strID += (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX) + (i != 3 ? ":" : "");
    }
    strID.toUpperCase();

    // Imprime o texto no serial.
    Serial.print("Identificador (UID) da tag: ");
    // Imprime o UID da TAG
    Serial.println(strID);

    // Imprime a versão da TAG
    rfid.PCD_DumpVersionToSerial();

    // Captura o TYPE da tag e o SAK
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    Serial.print(F("PICC type: "));
    Serial.print(rfid.PICC_GetTypeName(piccType));
    Serial.print(F(" (SAK "));
    Serial.print(rfid.uid.sak);
    Serial.println(F(")"));

    // Checa a compatibilidade da TAG
    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI && piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        Serial.println(F("Esta TAG não é compativel com o Leitor."));
        return;
    }

}

// Escreve os dados no bloco informado
void WriteDataToBlock(int blockNumber, byte blockData[]) {

    // Autenticando o bloco de dados desejado para acesso de leitura usando a chave A
    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNumber, &keyA, &(rfid.uid));

    // Autenticando o bloco de dados desejado para acesso de leitura usando a chave B
    // status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, blockNumber, &keyB, &(rfid.uid));

    if (status != MFRC522::STATUS_OK) {
        Serial.print("Autenticação falhou para a execução de escrita, erro: ");
        Serial.println(rfid.GetStatusCodeName(status));
        return;
    } else {
        Serial.println("Autenticação bem Sucedida.");
    }


    // Escrevendo dados no bloco
    status = rfid.MIFARE_Write(blockNumber, blockData, 16);
    if (status != MFRC522::STATUS_OK) {
        Serial.print("A escrita no bloco falhou, erro: ");
        Serial.println(rfid.GetStatusCodeName(status));
        return;
    } else {
        Serial.println("Os dados foram escritos com sucesso!");
    }

}

// Lê os dados do bloco informado
void ReadDataFromBlock(int blockNumber, byte readBlockData[]) {

    // Autenticando o bloco de dados desejado para acesso de leitura usando a chave A
    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNumber, &keyA, &(rfid.uid));

    // Autenticando o bloco de dados desejado para acesso de leitura usando a chave B
    // status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, blockNumber, &keyB, &(rfid.uid));

    if (status != MFRC522::STATUS_OK) {
        Serial.print("Autenticação falhou para a execução da leitura, erro: ");
        Serial.println(rfid.GetStatusCodeName(status));
        return;
    } else {
        Serial.println("Autenticação bem Sucedida.");
    }

    // Lendo os dados
    status = rfid.MIFARE_Read(blockNumber, readBlockData, &bufferLen);
    if (status != MFRC522::STATUS_OK) {
        Serial.print("A leitura falhou, erro: ");
        Serial.println(rfid.GetStatusCodeName(status));
        return;
    } else {
        Serial.println("Leitura do bloco concluida com sucesso!");
        Serial.print("\n");
        Serial.print("Bloco:");
        Serial.print(blockNumber);
        Serial.print(" Data: ");
        for (int j = 0; j < 16; j++) {
            Serial.write(readBlockData[j]);
        }
        Serial.print("\n");
    }

}