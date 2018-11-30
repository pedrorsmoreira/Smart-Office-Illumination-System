#include "node.h"

using namespace std;

Node::Node(){
    rho = 0;
    cost_best=0;

    for( int i = 0; i < 2; i++ ) {
      d[i] = 0;
      d_av[i] = 0;
      d_best[i]=0;
      y[i] = 0;
    }
     
    k[0] = 2;
    k[1] = 1;

    n = k.norm();
    m = n - pow( k[1], 2 );

    c = 0;
    o = 0;
    L = 0;
    index=0;
}                                                                                                                                                                                                                                                                                          

Node::Node(float _rho, int _c, int _o, int _L, int _index){
    rho = _rho;
    cost_best=10000;

    for( int i = 0; i < 2; i++ ) {
      d[i] = 0;
      d_best[i]=-1;
      d_av[i] = 1;
      y[i] = 0;
    }
     
    k[0] = 2;
    k[1] = 1;

    n = k.norm();
    m = n - pow( k[1], 2 );

    c = _c;
    o = _o;
    L = _L;
    index=_index;
}                                                                                                                                                                                                                                                                                          

bool Node::check_feasibility(Vector <float> _d){
  int tol= 0.001;
   if (_d[index] < 0-tol)
     return 0;
   if (_d[index] > 100+tol)
     return 0;
   if (_d*k < L-o-tol)
     return 0;

   return 1;
}

float Node::evaluate_cost(Vector <float> _d){
    
   return c*_d[index] + y*(_d-d_av) + (rho/2)*pow((_d-d_av).norm(),2);
}

void Node::updateBestCost(Vector <float> _d_solut){
  //+e para tirar a de baixo mas aqui e so para checkar o print
   bool solut=check_feasibility(_d_solut);
   Serial.println(solut);
   float cost_sol;
  if(check_feasibility(_d_solut))
  {
    cost_sol=evaluate_cost(_d_solut);
    if(cost_sol<cost_best)
    {
      d=_d_solut;
      cost_best=cost_sol;
      Serial.println(cost_best);   
    }
  }
  
  
}

void Node::Primal_solve(){
Vector <float> z = d_av*rho;
//Só para checkar
Serial.println(d_av[1]);
Serial.println(d_av[0]);
Serial.println(z[0]);
Serial.println(z[1]);

z[index]=z[index]-c;
Serial.println(z[0]);
Serial.println(z[1]);

//SOLUÇÃO EM QUE ESTA DENTRO DA FEASIBLE SET
//----------------------------------------------
Vector <float> d_solut= z*(1/rho);

Serial.println(d_solut[0]);
Serial.println(d_solut[1]);
Serial.println("sol_unconstrained");
updateBestCost(d_solut);



//--------------------------------------------------------------
//LINEAR BOUNDARY
//nao sei se é preciso isto
n=k.norm();
Serial.print("n ");
Serial.println((z*(1/rho))[0]);
Serial.println((z*(1/rho))[1]);
Serial.println((k*((1/n)*(o-L+((1/rho)*(z*k)))))[0]);
Serial.println((k*((1/n)*(o-L+((1/rho)*(z*k)))))[1]);


d_solut=(z*(1/rho))-(k*((1/n)*(o-L+((1/rho)*(z*k)))));
Serial.println("sol_boundary_linear");
Serial.println(d_solut[0]);
Serial.println(d_solut[1]);
//updateBestCost(d_solut);

//----------------------------------------------------
//check 0 boundary
d_solut=z*(1/rho);
d_solut[index]=0;
Serial.println("sol_boundary_0");
updateBestCost(d_solut);













  
}
