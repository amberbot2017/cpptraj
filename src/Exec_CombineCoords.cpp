#include "Exec_CombineCoords.h"
#include "CpptrajStdio.h"

void Exec_CombineCoords::Help() const {
  mprintf("\t<crd1> <crd2> ... [parmname <topname>] [crdname <crdname>]\n"
          "  Combine two or more COORDS data sets.\n");
}

Exec::RetType Exec_CombineCoords::Execute(CpptrajState& State, ArgList& argIn) {
  std::string parmname = argIn.GetStringKey("parmname");
  std::string crdname  = argIn.GetStringKey("crdname");
  // Get COORDS DataSets.
  std::vector<DataSet_Coords*> CRD;
  std::string setname = argIn.GetStringNext();
  while (!setname.empty()) {
    DataSet_Coords* ds = (DataSet_Coords*)State.DSL().FindCoordsSet( setname );
    if (ds == 0) {
      mprinterr("Error: %s: No COORDS set with name %s found.\n", argIn.Command(), setname.c_str());
      return CpptrajState::ERR;
    }
    CRD.push_back( ds );
    setname = argIn.GetStringNext();
  }
  if (CRD.size() < 2) {
    mprinterr("Error: %s: Must specify at least 2 COORDS data sets\n", argIn.Command());
    return CpptrajState::ERR;
  }
  // Only add the topology to the list if parmname specified
  bool addTop = true;
  Topology CombinedTop;
  if (parmname.empty()) {
    parmname = CRD[0]->Top().ParmName() + "_" + CRD[1]->Top().ParmName();
    addTop = false;
  }
  CombinedTop.SetParmName( parmname, FileName() );
  // TODO: Check Parm box info.
  size_t minSize = CRD[0]->Size();
  for (unsigned int setnum = 0; setnum != CRD.size(); ++setnum) {
    if (CRD[setnum]->Size() < minSize)
      minSize = CRD[setnum]->Size();
    CombinedTop.AppendTop( CRD[setnum]->Top() );
  }
  CombinedTop.Brief("Combined parm:");
  if (addTop) {
    if (State.AddTopology( CombinedTop, parmname )) return CpptrajState::ERR;
  }
  // Combine coordinates
  if (crdname.empty())
    crdname = CRD[0]->Meta().Legend() + "_" + CRD[1]->Meta().Legend();
  mprintf("\tCombining %zu frames from each set into %s\n", minSize, crdname.c_str());
  DataSet_Coords* CombinedCrd = (DataSet_Coords*)State.DSL().AddSet(DataSet::COORDS, crdname, "CRD");
  if (CombinedCrd == 0) {
    mprinterr("Error: Could not create COORDS data set.\n");
    return CpptrajState::ERR;
  }
  // FIXME: Only copying coords for now
  CombinedCrd->CoordsSetup( CombinedTop, CoordinateInfo() );
  Frame CombinedFrame( CombinedTop.Natom() * 3 );
  std::vector<Frame> InputFrames;
  for (unsigned int setnum = 0; setnum != CRD.size(); ++setnum)
    InputFrames.push_back( CRD[setnum]->AllocateFrame() );
  for (size_t nf = 0; nf != minSize; ++nf) {
    CombinedFrame.ClearAtoms();
    for (unsigned int setnum = 0; setnum != CRD.size(); ++setnum)
    {
      CRD[setnum]->GetFrame( nf, InputFrames[setnum] );
      for (int atnum = 0; atnum < CRD[setnum]->Top().Natom(); atnum++)
        CombinedFrame.AddXYZ( InputFrames[setnum].XYZ(atnum) );
    }
    CombinedCrd->AddFrame( CombinedFrame );
  }
/* FIXME: This code is fast but only works for DataSet_Coords_CRD
  Frame::CRDtype CombinedFrame( CombinedTop->Natom() * 3 );
  for (size_t nf = 0; nf != minSize; ++nf) {
    size_t offset = 0;
    for (unsigned int setnum = 0; setnum != CRD.size(); ++setnum)
    {
      size_t crd_offset = (size_t)CRD[setnum]->Top().Natom() * 3;
      std::copy( CRD[setnum]->CRD(nf).begin(), CRD[setnum]->CRD(nf).begin() + crd_offset,
                 CombinedFrame.begin() + offset );
      offset += crd_offset;
    }
    CombinedCrd->AddCRD( CombinedFrame );
  }
*/
  return CpptrajState::OK;
}
