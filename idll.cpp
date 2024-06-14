/*
** file: idll.cpp
**
** Integer Double Linked List
** UC: 21046 - EDAF @ UAb
** e-fólio A 2023-24
**
** Aluno: 2201529 - Bruno Ferreira
*/

// Defina:
//   Em idll.h as classes da estrutura de dados
//   Em idll.cpp a implementação dos métodos das classes da estrutura de dados

#include "idll.h"
#include <iostream>
#include <climits>

INode::INode(int item) : item(item), next(nullptr), prev(nullptr) {}

IDll::IDll() : n(0), head(nullptr), tail(nullptr) {}

IDll::~IDll() {
    if (!isEmpty()) {
        clear();
    }
}


bool IDll::isEmpty() {
    return n == 0;
}


// Implementação dos métodos...

void IDll::insert_0(int item) {
    // Cria um novo nó
    INode* newNode = new INode(item);
    newNode->next = head;
    if (head != nullptr) {
        head->prev = newNode;
    }
    head = newNode;
    if (tail == nullptr) {
        tail = newNode;
    }

    // Se a lista estiver vazia (ou seja, a cauda é nullptr), atualiza a cauda para ser o novo nó
    if (isEmpty()) {
        tail = head;
    }

    // Incrementa o contador do número de nós na lista
    n++;
}

void IDll::insert_end(int item) {
    INode* newNode = new INode(item);//cria um novo nó com o que foi fornecido 
    if (tail != nullptr) {
        tail->next = newNode;
        newNode->prev = tail;
    }
    tail = newNode;
    if (head == nullptr) {
        head = newNode;
    }
    n++;
}

void IDll::print_0() {
    if (head != nullptr) {
        std::cout << "Lista(0)= "<<head->item<<"\n";
    }
}

void IDll::print_end() {
    if (tail != nullptr) {
        std::cout << "Lista(end)= "<<tail->item<<"\n";
    }
}

void IDll::print() {
    if (isEmpty()) {
        std::cout << "Comando print: Lista vazia!\n";
        return;
    }
    INode* temp = head;
    std::cout << "Lista= ";
    while (temp != nullptr) {
        std::cout << temp->item;
        if (temp->next != nullptr) {
            std::cout << " ";  // adiciona espaço apenas se não for o último item
        }
        temp = temp->next;
    }
    std::cout << "\n";
}

void IDll::delete_0() {
    if (isEmpty()) {
        std::cout << "Comando delete_0: Lista vazia!\n";
        return;
    }
    INode* temp = head;
    head = head->next;
    if (head != nullptr) {
        head->prev = nullptr;
    } else {
        tail = nullptr;
    }
    delete temp;
    n--;
}

void IDll::delete_end() {
    
    if (isEmpty()) {
        std::cout << "Comando delete_end: Lista vazia!\n";
        return;
    }
    
    INode* temp = tail;
    tail = tail->prev;
    if (tail != nullptr) {
        tail->next = nullptr;
    } else {
        head = nullptr;
    }
    delete temp;
    n--;
    
}

void IDll::dim() {
    std::cout << "Lista tem " << n << " itens\n";
}


void IDll::clear() {
    if (isEmpty()) {
        std::cout << "Comando clear: Lista vazia!\n";
    } else {
        INode* current = head;
        while (current != nullptr) {
            INode* nextNode = current->next;
            delete current;
            current = nextNode;
        }
        head = tail = nullptr;
        n = 0;
    }
    
}



void IDll::find(int item) {
    INode* temp = head;//inicializa ponteiro temporario
    int pos = 0;
    while (temp != nullptr) {//percorre a lista enquanto temporario nao for null 
        if (temp->item == item) {
            std::cout << "Item " << item << " na posicao " << pos << "\n";
            return;
        }
        temp = temp->next;
        pos++;
    }
    std::cout << "Item " << item << " nao encontrado!\n";
}

void IDll::find_max() {
    if (isEmpty()) {
        std::cout << "Comando find_max: Lista vazia!\n";
        return;
    }
    INode* temp = head; //ponteiro temporário para o inicio da lista
    int maxItem = temp->item;
    int maxPos = 0;
    int pos = 0;
    while (temp != nullptr) {
        if (temp->item > maxItem) {
            maxItem = temp->item; // Inicializa maxItem com o item do primeiro nó e maxPos com 0
            maxPos = pos;
        }
        temp = temp->next;
        pos++; //incrementa a posisao 
    }
    std::cout << "Max Item " << maxItem << " na posicao " << maxPos << "\n";
}

void IDll::delete_pos(int pos) {
    if (pos < 0 || pos >= n) {
        std::cout << "Comando delete_pos: Posicao invalida!\n";
        return;
    }
    if (pos == 0) {
        delete_0();
    } else if (pos == n - 1) {
        delete_end();
    } else {
        INode* temp = head;
        for (int i = 0; i < pos; i++) {//Move o ponteiro temporário para o nó na posição especificada
            temp = temp->next;
        }
        temp->prev->next = temp->next;//atualiza os ponteiros dos nos adjacentes para ignorar o no a ser excluído
        temp->next->prev = temp->prev;
        delete temp;
        n--;
    }// Decrementa o contador 
}

void IDll::invert_range(int pos1, int pos2) {
    if (pos1 < 0 || pos2 >= n || pos1 > pos2) {
        std::cout << "Comando invert_range: Posicao invalida!\n";
        return;
    }
    int* items = new int[pos2 - pos1 + 1];//Cria um array para armazenar os itens na faixa especificada
    INode* temp = head;//Inicializa um ponteiro temporário para o início da lista
    for (int i = 0; i <= pos2; i++) {//Percorre a lista até a posição pos2
        if (i >= pos1) {// Se a posição atual estiver dentro da faixa especificada
            items[i - pos1] = temp->item;//Armazena o item do nó atual no array
        }
        temp = temp->next;// Move o ponteiro temporário para o próximo nó
    }
    temp = head;//Reinicializa o ponteiro temporário para o início da lista
    for (int i = 0; i <= pos2; i++) {
        if (i >= pos1) {
            temp->item = items[pos2 - i];
        }
        temp = temp->next;
    }
    delete[] items;
}








// EOF

