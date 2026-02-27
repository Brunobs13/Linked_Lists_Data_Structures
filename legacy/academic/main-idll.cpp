/*
** file: main-idll.cpp
**
** Integer Double Linked List
** UC: 21046 - EDAF @ UAb
** e-fólio A 2023-24
**
** Aluno: 2201529 - Bruno Ferreira
*/

// Defina neste ficheiro:
//   A entrada/saída de dados
//   As instâncias da classe da estrutura de dados
//   A implementação dos comandos através dos métodos da classe
//   Código auxiliar
//   Não utilize variáveis globais!
#include <sstream>
#include <iostream>
#include "idll.h"
using namespace std;

int main() {
    IDll list; //objeto list da class IDLL, chama o construtor com a lista vazia
    string line;
    while (getline(cin, line)) {
        if (line.empty() || line[0] == '#') continue;  // Ignora linhas vazias e comentários
        istringstream iss(line); //fluxo de entrada a partir da linha para facilitar a extração dos comandos e argumentos
        string cmd;// Variável que armazenar o comando
        iss >> cmd; // Extrai o comando da linha
        if (cmd == "insert_0") {//processar comando 
            int item;
            while (iss >> item) {
                list.insert_0(item);
            }
        } else if (cmd == "insert_end") {
            int item;
            while (iss >> item) {
                list.insert_end(item);
            }
        } else if (cmd == "print_0") {
            list.print_0();
        } else if (cmd == "print_end") {
            list.print_end();
        } else if (cmd == "print") {
            list.print();
        } else if (cmd == "delete_0") {
            list.delete_0();
        } else if (cmd == "delete_end") {
            list.delete_end();
        } else if (cmd == "dim") {
            list.dim();
        } else if (cmd == "clear") {
            list.clear();
        } else if (cmd == "find") {
            int item;
            if (iss >> item) {
                list.find(item);
            }
        } else if (cmd == "find_max") {
            list.find_max();
        } else if (cmd == "delete_pos") {
            int pos;
            if (iss >> pos) {
                list.delete_pos(pos);
            }
        } else if (cmd == "invert_range") {
            int pos1, pos2;
            if (iss >> pos1 >> pos2) {
                list.invert_range(pos1, pos2);
            }
        }
    }
    return 0;
}

// EOF