#include "gurobi_c++.h"
#include<iostream>
#include<vector>
#include"data.h"
using namespace std;

class benders_next {
public:
	my_data datas;
	GRBEnv env = GRBEnv();//enviroiment
	GRBModel master = GRBModel(env);
	GRBModel sub_problem = GRBModel(env);
	GRBLinExpr subobj=0;//������Ŀ�꺯��
	//��ż����
	GRBVar *u;//��ԴԼ����ż����
	GRBVar *v;//����Լ����ż����
	GRBVar **w;//x y Լ����ż����
	vector<double>u_source; //������Ŀ�꺯�����ż����u��Ӧϵ��
	vector<double>v_demand;//������Ŀ�꺯�����ż����v��Ӧϵ��
	vector<vector<double>>w_M;//��������Ŀ�꺯�����ż����w��Ӧϵ��
	GRBConstr **sub_con;//�������Լ��
	vector<vector<double>>x_num;//ԭ�����xֵ
	GRBVar **y;//������ı���
	GRBVar sub_cost;//�������Ŀ�꺯��,��Ӧ�������q
	vector<vector<int>>y_1;//�������ʼy��ֵ
	double UB;
	double LB;
	benders_next(my_data datas) {
		this->datas = datas;
	}
	/*~benders_next() {
		delete u;
		delete v;
		for (int i = 0; i < datas.source_size; i++)
		{
			delete[] w[i];
			delete[] y[i];
		}
		delete[]w;
		delete[]y;
	}*/
	void create_model() {
		try {
			y_1 = vector<vector<int>>(datas.source_size, vector<int>(datas.demand_size, 1));
			u = new GRBVar[datas.source_size];
			v = new GRBVar[datas.demand_size];
			w = new GRBVar*[datas.source_size];
			for (int i = 0; i < datas.source_size; i++) {
				w[i] = new GRBVar[datas.demand_size];
			}
			u_source.resize(datas.source_size);
			v_demand.resize(datas.demand_size);
			w_M = vector<vector<double>>(datas.source_size, vector<double>(datas.demand_size,0));
			y = new GRBVar*[datas.source_size];
			for (int i = 0; i < datas.source_size; i++) {
				y[i] = new GRBVar[datas.demand_size];
			}
			sub_con = new GRBConstr*[datas.source_size];
			for (int i = 0; i < datas.source_size; i++) {
				sub_con[i] = new GRBConstr[datas.demand_size];
			}
			x_num= vector<vector<double>>(datas.source_size, vector<double>(datas.demand_size, 0));
			//��Ӳ���
			sub_cost = master.addVar(0.0, INT_MAX, 0, GRB_CONTINUOUS, "sub cost");
			for (int i = 0; i < datas.source_size; i++) {
				u[i] = sub_problem.addVar(0, INFINITY,0, GRB_CONTINUOUS, "u_" + to_string(i));
			}
			for (int i = 0; i < datas.demand_size; i++) {
				v[i] = sub_problem.addVar(0, INFINITY, 0, GRB_CONTINUOUS, "v_" + to_string(i));
			}
			for (int i = 0; i < datas.source_size; i++) {
				for (int j = 0; j < datas.demand_size; j++) {
					y[i][j] = master.addVar(0, 1, 0, GRB_BINARY, "y_" + to_string(i) + to_string('_') + to_string(j));
					w[i][j] = sub_problem.addVar(0, INFINITY, 0, GRB_CONTINUOUS, "w_" + to_string(i) + to_string('_') + to_string(j));
				}
			}
			//������
			GRBLinExpr master_obj = 0;
			for (int i=0; i < datas.source_size; i++) {
				for (int j = 0; j < datas.demand_size; j++) {
					master_obj += datas.fixed_c[i][j] * y[i][j];
				}
			}
			master.setObjective((master_obj + sub_cost),GRB_MINIMIZE);
			//������
			//������Ŀ�꺯��
			GRBLinExpr sub_object = 0;
			for (int i = 0; i < datas.source_size; i++) {
				u_source[i] = -datas.supply[i];
				sub_object += u[i] * u_source[i];
				//subobj += u[i] * u_source[i];
			}
			for (int i = 0; i < datas.demand_size; i++) {
				v_demand[i] = datas.demand[i];
				sub_object += v[i] * v_demand[i];
				//subobj += v[i] * v_demand[i];
			}
			for (int i = 0; i < datas.source_size; i++) {
				for (int j = 0; j < datas.demand_size; j++) {
					w_M[i][j] = -datas.M[i][j];
					sub_object+= w_M[i][j] * y_1[i][j] * w[i][j];
					//subobj += w_M[i][j] * y_1[i][j] * w[i][j];
				}
			}
			sub_problem.setObjective(sub_object, GRB_MAXIMIZE);
			//������Լ��
			for (int i = 0; i < datas.source_size; i++) {
				for (int j = 0; j < datas.demand_size; j++) {
					GRBLinExpr e = 0;
					e = -u[i] + v[j] - w[i][j];
					sub_con[i][j]= sub_problem.addConstr(e <= datas.c[i][j], "C_" + to_string(i) + "_" + to_string(j));
				}
			}
			sub_problem.set(GRB_IntParam_Presolve, GRB_PRESOLVE_OFF);
			sub_problem.getEnv().set(GRB_IntParam_OutputFlag, 0);
			master.getEnv().set(GRB_IntParam_OutputFlag, 0);

		}
		catch (GRBException e) {
			cout << e.getMessage() << endl;
		}
		catch (...) {
			cout << "wrong" << endl;
		}
	
	}
	void benders_solve() {
		UB = INT_MAX;
		LB = INT_MIN;
		try {
			while (UB>LB+FUZZ) {
				//�����ɳڵ��������е� ����yֵ����������Ŀ�꺯��
				GRBLinExpr subobj = 0;
				for (int i = 0; i < datas.source_size; i++) {
					subobj += u_source[i] * u[i];
				}
				for (int i = 0; i < datas.demand_size; i++) {
					subobj += v_demand[i] * v[i];
				}
				for (int i = 0; i < datas.source_size; i++) {
					for (int j = 0; j < datas.demand_size; j++) {
						subobj += w_M[i][j] * w[i][j] * y_1[i][j];
					}
				}
				sub_problem.setObjective(subobj, GRB_MAXIMIZE);
				//sub_problem.set(GRB_IntParam_InfUnbdInfo, 1);
				sub_problem.optimize();
				for (int i = 0; i < datas.source_size; i++) {
					for (int j = 0; j < datas.demand_size; j++) {
						x_num[i][j] = sub_con[i][j].get(GRB_DoubleAttr_Pi);//�õ���ż����pi
					}
				}
				auto status = sub_problem.get(GRB_IntAttr_Status);//�õ�����������״̬
				
				//��Ӽ����ߵ�Լ��
				if (status == GRB_UNBOUNDED) {

					GRBLinExpr e = 0;
					for (int i = 0; i < datas.source_size; i++) {
						e += u[i].get(GRB_DoubleAttr_UnbdRay)*u_source[i];
					}
					for (int i = 0; i < datas.demand_size; i++) {
						e += v[i].get(GRB_DoubleAttr_UnbdRay)*v_demand[i];
					}
					for (int i = 0; i < datas.source_size; i++) {
						for (int j = 0; j < datas.demand_size; j++) {
							e += w[i][j].get(GRB_DoubleAttr_UnbdRay)*w_M[i][j] * y[i][j];
						}
					}
					master.addConstr(e <= 0);
				}
				//��Ӽ����Լ��
				else if (status == GRB_OPTIMAL) {
					GRBLinExpr e = 0;
					for (int i = 0; i < datas.source_size; i++) {
						e += u[i].get(GRB_DoubleAttr_X)*u_source[i];
					}
					for (int i = 0; i < datas.demand_size; i++) {
						e += v[i].get(GRB_DoubleAttr_X)*v_demand[i];
					}

					for (int i = 0; i < datas.source_size; i++) {
						for (int j = 0; j < datas.demand_size; j++) {
							e += y[i][j] * w_M[i][j] * w[i][j].get(GRB_DoubleAttr_X);
						}
					}
					master.addConstr(e <= sub_cost);
					double sum_cij_fij = 0;
					for (int i = 0; i < datas.source_size; i++) {
						for (int j = 0; j < datas.demand_size; j++) {
							sum_cij_fij += datas.fixed_c[i][j] * y_1[i][j];
						}
					}
					cout << "UB " << UB << endl;
					UB = min(UB, (sum_cij_fij + sub_problem.get(GRB_DoubleAttr_ObjVal)));
				}
				else
				{
					//error
				}
				//���������
				master.optimize();
				LB = master.get(GRB_DoubleAttr_ObjVal);
				cout << "LB:" << LB << endl;
				for (int i = 0; i < datas.source_size; i++) {
					for (int j = 0; j < datas.demand_size; j++) {
						double aa = y[i][j].get(GRB_DoubleAttr_X);
						if (aa > 0.5) {
							y_1[i][j] = 1;
						}
						else {
							y_1[i][j] = 0;
						}
					}
				}

			}
		}
		
		catch (GRBException E) {
			cout << E.getMessage() << endl;
		}
		catch (...) {
			cout << "error" << endl;
		}
		cout << UB << endl;
		cout << LB << endl;
	}
};

int main() {

	my_data datas;
	datas.read_data();
	benders_next bn(datas);
	bn.create_model();
	bn.benders_solve();
}

