#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <bits/stdc++.h>
#include <vector>
using namespace std;

const int LOADCYCLES=3;
const int MULCYCLES=10;
const int ADDCYCLES=2;
const int DIVCYCLES=40;

struct Ins{
    string op;
    int j;
    int k;
    int place;
    int rs;
    int dest; //index of register destination
    int issued=-1;
    int ex_start=-1;
    int ex_end=-1;
    int written=-1;
    double result;
    int RSnum; //index of RS that holds instruction
    Ins(string op,int dest, int j, int k, int place):
        op(op), j(j), k(k), dest(dest), place(place) {};

    void print(){
        cout<<place<<": "<<op<<" "<<dest<<" "<<j<<" "<<k<<endl;
    }
};

struct RS{
    string op;
    string unitType;
    int num; //in order of the RS in the RS vector
    int counter=0; //for counting the cycles in execution
    double Vj=NAN;
    double Vk=NAN;
    int Qj=-1; //index of RS for Qj
    int Qk=-1;
    int A_offset=0;
    int A_reg=-1;
    int ins=-1; //which instruction is being executed
    bool busy=false;
    bool resultReady=false;
    RS(){};
    RS(string unitType, int num): unitType(unitType),num(num){};
};

void issue(ofstream& outputFile, int cyclenum, vector<Ins>& instructions, vector<RS>& RStations, unordered_map<int,int>& RegStats,vector<double> Registers);
void execute(ofstream& outputFile,unordered_map<string,int> cycle_map, int cyclenum, vector<Ins>& instructions, vector<RS>& RStations, unordered_map<int,int>& RegStats,vector<double> Registers, unordered_map<int,double>& mem);
void writeResult(ofstream& outputFile,int& writebacks, int cyclenum, vector<Ins>& instructions, vector<RS>& RStations, unordered_map<int,int>& RegStats,vector<double>& Registers,unordered_map<int,double>& mem);

template<typename T> string formatL(T t, const int& width){
    stringstream ss;
    ss << fixed << left;
    ss.fill(' ');        // fill space around displayed #
    ss.width(width);     // set  width around displayed #
    if (is_same<T,double>::value) ss.precision(2);
    ss << t;
    return ss.str();
}

//==============MAIN DRIVER================//
int main(int argc, char *argv[]) {
    int num_ins=0;
    int j, k, dest, index;
    int place=0;
    string op; string destStr; string jStr; string kStr;
    unordered_map<string,int> cycle_map;
    cycle_map["ADDD"]=ADDCYCLES;
    cycle_map["SUBD"]=ADDCYCLES;
    cycle_map["MULTD"]=MULCYCLES;
    cycle_map["DIVD"]=DIVCYCLES;
    cycle_map["LD"]=LOADCYCLES;
    cycle_map["SD"]=LOADCYCLES;

    //input format: ./Tomasulo [input file]
    //parse the file, store each instruction into struct
    string line;
    ifstream file(argv[1]);
    vector<Ins> instructions;
    while(getline(file,line)){
        num_ins++;
        istringstream stream(line);
        stream>>op>>destStr>>jStr>>kStr;
        index=1;
        if(op=="LD"||op=="SD") index=0;

        dest=atoi(destStr.substr(1).c_str());
        j=atoi(jStr.substr(index).c_str());
        k=atoi(kStr.substr(1).c_str());
        Ins ins(op,dest,j,k,place);
        instructions.push_back(ins);
        ins.print();
        place++;
    }
    cout<<num_ins<<" instructions."<<endl;

    //initialize Registers, RegStats, reservation stations, and memory
    //RegStats is hashtable of {key=register index, val=Qi} pair
    vector<double> Registers{0, 1,2,3,4,5,6,7,8,9,10,11};
    vector<RS> RStations{RS("ADD1",0),RS("ADD2",1),RS("ADD3",2),RS("MULT1",3),RS("MULT2",4),RS("MULT3",5),RS("LD1",6),RS("LD2",7),RS("LD3",8)};
    unordered_map<int,int> RegStats({{0,-1},{1,-1},{2,-1},{3,-1},{4,-1},{5,-1},{6,-1},{7,-1},{8,-1},{9,-1},{10,-1},{11,-1}});
    unordered_map<int,double> mem({{0,10},{1,11},{2,22},{3,33},{48,5},{36,8}});

    //create output file to write to
    ofstream outputFile;
    string filename(argv[1]);
    string filename2=filename.substr(0,filename.length()-4);
    string newFile=filename2+"_results.txt";
    outputFile.open(newFile);

    //loop cycles until last instruction is done writing
    int writebacks=0;
    int cyclenum=1;
    while(writebacks!=num_ins){
        outputFile<<"CYCLE "<<cyclenum<<endl;
        issue(outputFile, cyclenum,instructions,RStations,RegStats,Registers);
        execute(outputFile, cycle_map,cyclenum,instructions,RStations,RegStats,Registers,mem);
        writeResult(outputFile, writebacks,cyclenum,instructions,RStations,RegStats,Registers,mem);
        cyclenum++;

        //print reservation stations, register content, and register status for each cycle
        outputFile<<formatL("RS",3)<<"|"<<formatL("Busy",4)<<"|"<<formatL("Op",6)<<"|"<<formatL("Vj",7)<<"|"<<formatL("Vk",7)<<"|"<<formatL("Qj",5)<<"|"<<formatL("Qk",5)<<"|"<<formatL("Address",10)<<endl;
        for(int rs=0; rs<RStations.size();rs++){
            outputFile<<formatL(rs,3)<<"|"<<formatL(RStations[rs].busy,4)<<"|"<<formatL(RStations[rs].op,6)<<"|"<<formatL(RStations[rs].Vj,7)<<"|"<<formatL(RStations[rs].Vk,7)<<"|"<<formatL(RStations[rs].Qj,5)<<"|"<<formatL(RStations[rs].Qk,5)<<"|"<<RStations[rs].A_offset<<"+R"<<RStations[rs].A_reg<<endl;

        }

        outputFile<<"\nREGISTER STATUS"<<endl;
        for(int reg=0; reg<Registers.size();reg++){
            outputFile<<formatL(reg,4)<<":"<<formatL(Registers[reg],7)<<"|"<<formatL("Qi=",4);
            if (RegStats[reg]>-1) outputFile<<RStations[RegStats[reg]].unitType.c_str()<<endl;
            else outputFile<<endl;
        }
        outputFile<<"================================================"<<endl;

    }

    //display issue/ex/wb table
    outputFile<<"\n================SUMMARY TABLE===================="<<endl;
    outputFile<<formatL("Ins",6)<<"|"<<formatL("Issue",6)<<"|"<<formatL("Ex start",8)<<"|"<<formatL("Ex end",6)<<"|"<<formatL("Write",6)<<endl;
    for(int i=0;i<num_ins;i++){
        outputFile<<formatL(instructions[i].op.c_str(),6)<<"|"<<formatL(instructions[i].issued,6)<<"|"<<formatL(instructions[i].ex_start,8)<<"|"<<formatL(instructions[i].ex_end,6)<<"|"<<formatL(instructions[i].written,6)<<endl;
    }

    outputFile.close();
    return 0;
}


//===============FUNCTIONS===============//
void issue(ofstream& outputFile, int cyclenum, vector<Ins>& instructions, vector<RS>& RStations, unordered_map<int,int>& RegStats,vector<double> Registers){
    int rs=-1;
    //find the first instruction that has not been issued
    for(int i=0;i<instructions.size();i++) {
        if(instructions[i].issued==-1) {
            string curr_op = instructions[i].op;
            if ((curr_op=="LD") || (curr_op=="SD")) {
                for (int r:{6,7,8}) {
                    if (!RStations[r].busy) {
                        rs=r;
                        instructions[i].issued = cyclenum;
                        outputFile<<"  Issued instruction "<<i+1<<" in cycle "<<cyclenum<<endl;
                        instructions[i].rs=rs;
                        RStations[rs].busy = true;
                        RStations[rs].ins=i;
                        RStations[rs].op=instructions[i].op;

                        //set the address value of RS and Qi
                        RStations[rs].A_offset = instructions[i].j;
                        RStations[rs].A_reg = instructions[i].k;
                        if(curr_op=="LD") RegStats[instructions[i].dest]=rs;

                        break;
                    }

                }
            }
            else {
                if ((curr_op=="ADDD") || (curr_op=="SUBD")) {
                    for (int r:{0,1,2}) {

                        if (!RStations[r].busy) {
                            rs=r;
                            instructions[i].issued = cyclenum;
                            outputFile<<"  Issued instruction "<<i+1<<" in cycle "<<cyclenum<<endl;
                            instructions[i].rs=rs;
                            RStations[rs].ins=i;
                            RStations[rs].op=instructions[i].op;
                            RStations[rs].busy = true;

                            RegStats[instructions[i].dest]=RStations[rs].num;

                            break;
                        }

                    }
                }
                if (curr_op=="MULTD" || curr_op=="DIVD") {
                    for (int r:{3,4,5}) {

                        if (!RStations[r].busy) {
                            rs=r;
                            instructions[i].issued = cyclenum;
                            outputFile<<"  Issued instruction "<<i+1<<" in cycle "<<cyclenum<<endl;
                            instructions[i].rs=rs;
                            RStations[rs].ins=i;
                            RStations[rs].op=instructions[i].op;
                            RStations[rs].busy = true;
                            RegStats[instructions[i].dest]=RStations[rs].num;

                            break;
                        }

                    }
                }

                //check if operand is available for Vj and Vk, otherwise set Qj and Qk
                if(rs>-1) {
                    if (RegStats[instructions[i].j]<0 || instructions[i].dest==instructions[i].j) {
                        RStations[rs].Vj = Registers[instructions[i].j];

                    } else {
                        RStations[rs].Qj = RegStats[instructions[i].j];
                    }
                    if (RegStats[instructions[i].k]<0 || instructions[i].dest==instructions[i].k) {
                        RStations[rs].Vk = Registers[instructions[i].k];

                    } else {
                        RStations[rs].Qk = RegStats[instructions[i].k];
                    }
                }

            }
        break;
        }
    }
}

void execute(ofstream& outputFile, unordered_map<string,int> cycle_map, int cyclenum, vector<Ins>& instructions, vector<RS>& RStations, unordered_map<int,int>& RegStats,vector<double> Registers, unordered_map<int,double>& mem){
    //go through busy reservation stations
    for(int r=0;r<RStations.size();r++){
        if(RStations[r].busy){
            Ins* instruction=&instructions[RStations[r].ins];
            if(cyclenum>instruction->issued && instruction->ex_end<0){ //issue latency finished and not done executing
            //check if operands available and increase the execution counter
            //for add, mult, and div
                if((instruction->op!="LD")&&(instruction->op!="SD")){
                    if((!::isnan(RStations[r].Vj)) && (!::isnan(RStations[r].Vk))){
                        if (instruction->ex_start < 0) {
                            instruction->ex_start = cyclenum;
                             outputFile<< "  Started executing instruction " << RStations[r].ins + 1 << endl;
                            if (instruction->op == "ADDD")
                                instruction->result = RStations[r].Vj + RStations[r].Vk;
                            if (instruction->op == "SUBD")
                                instruction->result = RStations[r].Vj - RStations[r].Vk;
                            if (instruction->op == "MULTD")
                                instruction->result = RStations[r].Vj * RStations[r].Vk;
                            if (instruction->op == "DIVD")
                                instruction->result = RStations[r].Vj / RStations[r].Vk;
                        }

                    }
                }

                //for load and store - calculate address, check that no registers have source k in their Qi
                if((instruction->op=="LD")||(instruction->op=="SD")){
                    if((RegStats[instruction->k]<0) || (RegStats[instruction->k]>-1 && instructions[RStations[RegStats[instruction->k]].ins].issued>=instruction->issued)){ //if Qi empty or if this ins was issued before the one completing Qi
                        if(instruction->ex_start<0) {
                            instruction->ex_start = cyclenum;
                            outputFile << "  Started executing instruction " << RStations[r].ins + 1 << endl;
                            instruction->result = mem[RStations[r].A_offset+(int)Registers[instruction->k]];
                        }
                    }
                }

                //increase execution counter
                //complete execution and set resultReady if execution cycles completed this cycle
                if((RStations[r].counter<cycle_map[instruction->op]) && (instruction->ex_start>0)) {
                    RStations[r].counter++;
                }
                if(RStations[r].counter==cycle_map[instruction->op] && !RStations[r].resultReady) {
                    RStations[r].resultReady=true;
                    instruction->ex_end=cyclenum;
                    outputFile<<"  Finished executing instruction "<<RStations[r].ins+1<<endl;
                } else{
                    if((instruction->ex_start>0) && (instruction->ex_start<cyclenum)){
                        outputFile<<"  Currently executing instruction "<<RStations[r].ins+1<<endl;
                    }
                }

            }

        }
    }
}

void writeResult(ofstream& outputFile, int& writebacks,int cyclenum, vector<Ins>& instructions, vector<RS>& RStations,unordered_map<int,int>& RegStats,vector<double>& Registers,unordered_map<int,double>& mem){
    //loop through, get the first RS with a result ready
    //only 1 result written on CDB per cycle allowed
    for(RS &rs:RStations){
        if(rs.busy) {
            Ins *instruction = &instructions[rs.ins];
            if (rs.resultReady && instruction->ex_end < cyclenum && instruction->written < 0) { //result ready and execute latency finished, can start writing
                //write to destination register and clear Qi
                if(rs.op!="SD") {
                    //cout << "writing the result " << instruction->result << " to destination register "<< instruction->dest << ", clearing Qi." << endl;
                    if(RegStats[instruction->dest]>-1) {
                        if (RStations[RegStats[instruction->dest]].unitType == rs.unitType)
                            Registers[instruction->dest] = instruction->result;
                    }
                    RegStats[instruction->dest] = -1;
                } else{
                    //cout << "writing the result "<< instruction->result <<"to memory address "<< instruction->dest<<endl;
                    mem[instruction->dest]=instruction->result;
                }
                //write to reservation stations waiting for the result (any Qj, Qk, or address reg RS = rs number)
                for (RS &rs2:RStations) {
                    if (rs2.Qj == rs.num) {
                        outputFile<<"writing to Vj of RS "<<rs2.num<<endl;
                        rs2.Qj = -1;
                        rs2.Vj = instruction->result;
                    }
                    if (rs2.Qk == rs.num) {
                        rs2.Qk = -1;
                        rs2.Vk = instruction->result;
                    }
                }

                //clear busy and resultReady flag, set written tag, clear RS fields, clear counter
                instruction->written = cyclenum;
                outputFile<<"  Instruction "<<rs.ins+1<<" written on cycle "<<cyclenum<<endl;
                rs.op="";
                rs.resultReady = false;
                rs.ins = -1;
                rs.busy = false;
                rs.counter = 0;
                rs.Qj = -1;
                rs.Qk = -1;
                rs.Vj = NAN;
                rs.Vk = NAN;
                rs.A_reg = -1;
                rs.A_offset = 0;

                writebacks++;
                break;
            }
        }
    }
}

