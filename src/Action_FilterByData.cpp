#include "Action_FilterByData.h"
#include "CpptrajStdio.h"

void Action_FilterByData::Help() const {
  mprintf("\t<dataset arg> min <min> max <max> [out <file> [name <setname>]]\n"
          "  For all following actions, only allow frames that are between <min>\n"
          "  and <max> of data sets in <dataset arg>. There must be at least\n"
          "  one <min> and <max> argument, and can be as many as there are\n"
          "  specified data sets.\n");
}

// Action_FilterByData::Init()
Action::RetType Action_FilterByData::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
  maxmin_ = init.DSL().AddSet( DataSet::INTEGER, actionArgs.GetStringKey("name"), "Filter" );
  if (maxmin_ == 0) return Action::ERR;
  DataFile* maxminfile = init.DFL().AddDataFile( actionArgs.GetStringKey("out"), actionArgs );
  if (maxminfile != 0)
    maxminfile->AddDataSet( maxmin_ );
  // Get min and max args.
  while (actionArgs.Contains("min"))
    Min_.push_back( actionArgs.getKeyDouble("min", 0.0) );
  while (actionArgs.Contains("max"))
    Max_.push_back( actionArgs.getKeyDouble("max", 0.0) );
  if (Min_.empty()) {
    mprinterr("Error: At least one 'min' arg must be specified.\n");
    return Action::ERR;
  }
  if (Max_.empty()) {
    mprinterr("Error: At least one 'max' arg must be specified.\n");
    return Action::ERR;
  }
  if (Min_.size() != Max_.size()) {
    mprinterr("Error: # of 'min' args (%zu) != # of 'max' args (%zu)\n",
              Min_.size(), Max_.size());
    return Action::ERR;
  }
  // Get DataSets from remaining arguments
  Dsets_.AddSetsFromArgs( actionArgs.RemainingArgs(), init.DSL() );

  if (Dsets_.empty()) {
    mprinterr("Error: No data sets specified.\n");
    return Action::ERR;
  }

  if ( Dsets_.size() < Min_.size() ) {
    mprinterr("Error: More 'min'/'max' args (%zu) than data sets (%zu).\n",
              Min_.size(), Dsets_.size());
    return Action::ERR;
  }
  if ( Dsets_.size() > Min_.size() ) {
    unsigned int Nremaining = Dsets_.size() - Min_.size();
    double useMin = Min_.back();
    double useMax = Max_.back();
    mprintf("Warning: More data sets than 'min'/'max' args.\n"
            "Warning:  Using min=%f and max=%f for last %zu data sets.\n",
            useMin, useMax, Nremaining);
    for (unsigned int ds = 0; ds < Nremaining; ++ds) {
      Min_.push_back( useMin );
      Max_.push_back( useMax );
    }
  }

  mprintf("    FILTER: Filtering out frames using %zu data sets.\n", Dsets_.size());
  for (unsigned int ds = 0; ds < Dsets_.size(); ds++)
    mprintf("\t%.4f < '%s' < %.4f\n", Min_[ds], Dsets_[ds]->legend(), Max_[ds]);
  if (maxminfile != 0)
    mprintf("\tFilter frame info will be written to %s\n", maxminfile->DataFilename().full());
# ifdef MPI
  if (init.TrajComm().Size() > 1)
    mprintf("Warning: Trajectories written after 'filter' may have issues if\n"
            "Warning:   the number of threads writing is > 1 (currently %i threads)\n",
            init.TrajComm().Size());
# endif
  return Action::OK;
}

// Action_FilterByData::DoAction()
Action::RetType Action_FilterByData::DoAction(int frameNum, ActionFrame& frm)
{
  static int ONE = 1;
  static int ZERO = 0;
  // Check if frame is within max/min
  for (unsigned int ds = 0; ds < Dsets_.size(); ++ds)
  {
    double dVal = Dsets_[ds]->Dval(frm.TrajoutNum());
    //mprintf("DBG: maxmin[%u]: dVal = %f, min = %f, max = %f\n",ds,dVal,Min_[ds],Max_[ds]);
    // If value from dataset not within min/max, exit now.
    if (dVal < Min_[ds] || dVal > Max_[ds]) {
      maxmin_->Add( frameNum, &ZERO );
      return Action::SUPPRESS_COORD_OUTPUT;
    }
  }
  maxmin_->Add( frameNum, &ONE );
  return Action::OK;
}

size_t Action_FilterByData::DetermineFrames() const {
  if (Dsets_.empty()) return 0;
  size_t nframes = Dsets_[0]->Size();
  for (Array1D::const_iterator it = Dsets_.begin(); it != Dsets_.end(); ++it)
  {
    if ((*it)->Size() < nframes)
      nframes = (*it)->Size();
  }
  // Warn if any datasets are larger.
  for (Array1D::const_iterator it = Dsets_.begin(); it != Dsets_.end(); ++it)
  {
    if ((*it)->Size() > nframes)
      mprintf("Warning: '%s' size %zu is larger than other sets; only processing %zu\n",
              (*it)->legend(), (*it)->Size(), nframes);
  }
  return nframes;
}
