#ifndef INC_ACTION_DIPOLE_H
#define INC_ACTION_DIPOLE_H
#include "Action.h"
#include "DataSet_GridFlt.h"
#include "GridAction.h"
class Action_Dipole : public Action, private GridAction {
  public:
    Action_Dipole();
    static DispatchObject* Alloc() { return (DispatchObject*)new Action_Dipole(); }
    static void Help();
  private:
    Action::RetType Init(ArgList&, ActionInit&, int);
    Action::RetType Setup(ActionSetup&);
    Action::RetType DoAction(int, ActionFrame&);
    void Print();

    DataSet_GridFlt* grid_;
    std::vector<Vec3> dipole_;
    CpptrajFile* outfile_;
    CharMask mask_;
    double max_;
    Topology* CurrentParm_;
};
#endif
