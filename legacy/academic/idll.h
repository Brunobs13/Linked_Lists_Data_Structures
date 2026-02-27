/*
** file: idll.h
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
// Não altere o nome das classes nem dos atributos obrigatórios!

// definir nó
struct INode {
   // atributos obrigatórios
   int item;             // informação em cada nó
   INode* next;          // apontador para o próximo nó
   INode* prev;          // apontador para o nó anterior

   // Construtor
   INode(int item);
};


//lista duplamente ligada
class IDll {
private:
   // atributos obrigatórios
   int n;                // dimensão atual da lista
   INode* head;          // apontador para o início da lista
   INode* tail;          // apontador para o fim da lista

public:
   // Construtor e destrutor
   IDll();
   ~IDll();

   // Métodos correspondentes aos comandos
   bool isEmpty();
   void insert_0(int item);
   void insert_end(int item);
   void print_0();
   void print_end();
   void print();
   void delete_0();
   void delete_end();
   void dim();
   void clear();
   void find(int item);
   void find_max();
   void delete_pos(int pos);
   void invert_range(int pos1, int pos2);
};
// EOF
