#include <cmath> // sqrt
#include "Action_Diffusion.h"
#include "CpptrajStdio.h"
#include "StringRoutines.h" // validDouble

// CONSTRUCTOR
Action_Diffusion::Action_Diffusion() :
  avg_x_(0), avg_y_(0), avg_z_(0), avg_r_(0), avg_a_(0),
  printIndividual_(false),
  time_(1.0),
  hasBox_(false),
  debug_(0),
  outputx_(0), outputy_(0), outputz_(0), outputr_(0), outputa_(0),
  boxcenter_(0.0),
  masterDSL_(0)
{}

static inline void ShortHelp() {
  mprintf("\t[{out <filename> | separateout <suffix>}] [time <time per frame>]\n"
          "\t[<mask>] [<set name>] [individual]\n");
}

void Action_Diffusion::Help() {
  ShortHelp();
  mprintf("  Compute a mean square displacement plot for the atoms in the <mask>.\n"
          "  The following files are produced:\n"
          "    x_<suffix>: Mean square displacement(s) in the X direction (in Å^2).\n"
          "    y_<suffix>: Mean square displacement(s) in the Y direction (in Å^2).\n"
          "    z_<suffix>: Mean square displacement(s) in the Z direction (in Å^2).\n"
          "    r_<suffix>: Overall mean square displacement(s) (in Å^2).\n"
          "    a_<suffix>: Total distance travelled (in Å).\n");
}

static inline int CheckTimeArg(double dt) {
  if (dt <= 0.0) {
    mprinterr("Error: Diffusion time per frame incorrectly specified, must be > 0.0.\n");
    return 1;
  }
  return 0;
}

// Action_Diffusion::Init()
Action::RetType Action_Diffusion::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
  debug_ = debugIn;
  // Determine if this is old syntax or new.
  if (actionArgs.Nargs() > 2 && actionArgs.ArgIsMask(1) && validDouble(actionArgs[2]))
  {
    // Old syntax: <mask> <time per frame> [average] [<prefix>]
    printIndividual_ = !(actionArgs.hasKey("average"));
    mprintf("Warning: Deprecated syntax for 'diffusion'. Consider using new syntax:\n");
    ShortHelp();
    mask_.SetMaskString( actionArgs.GetMaskNext() );
    time_ = actionArgs.getNextDouble(1.0);
    if (CheckTimeArg(time_)) return Action::ERR;
    std::string outputNameRoot = actionArgs.GetStringNext();
    // Default filename: 'diffusion'
    if (outputNameRoot.empty())
      outputNameRoot.assign("diffusion");
    // Open output files
    ArgList oldArgs("prec 8.3 noheader");
    DataFile::DataFormatType dft = DataFile::DATAFILE;
    outputx_ = init.DFL().AddDataFile(outputNameRoot+"_x.xmgr", dft, oldArgs);
    outputy_ = init.DFL().AddDataFile(outputNameRoot+"_y.xmgr", dft, oldArgs);
    outputz_ = init.DFL().AddDataFile(outputNameRoot+"_z.xmgr", dft, oldArgs);
    outputr_ = init.DFL().AddDataFile(outputNameRoot+"_r.xmgr", dft, oldArgs);
    outputa_ = init.DFL().AddDataFile(outputNameRoot+"_a.xmgr", dft, oldArgs);
  } else {
    // New syntax: [{separateout <suffix> | out <filename>}] [time <time per frame>]
    //             [<mask>] [<set name>] [individual]
    printIndividual_ = actionArgs.hasKey("individual");
    std::string suffix = actionArgs.GetStringKey("separateout");
    std::string outname = actionArgs.GetStringKey("out");
    if (!outname.empty() && !suffix.empty()) {
      mprinterr("Error: Specify either 'out' or 'separateout', not both.\n");
      return Action::ERR;
    }
    time_ = actionArgs.getKeyDouble("time", 1.0);
    if (CheckTimeArg(time_)) return Action::ERR;
    mask_.SetMaskString( actionArgs.GetMaskNext() );
    // Open output files.
    if (!suffix.empty()) {
      FileName FName( suffix );
      outputx_ = init.DFL().AddDataFile(FName.DirPrefix()+"x_"+FName.Base(), actionArgs);
      outputy_ = init.DFL().AddDataFile(FName.DirPrefix()+"y_"+FName.Base(), actionArgs);
      outputz_ = init.DFL().AddDataFile(FName.DirPrefix()+"z_"+FName.Base(), actionArgs);
      outputr_ = init.DFL().AddDataFile(FName.DirPrefix()+"r_"+FName.Base(), actionArgs);
      outputa_ = init.DFL().AddDataFile(FName.DirPrefix()+"a_"+FName.Base(), actionArgs);
    } else if (!outname.empty()) {
      outputr_ = init.DFL().AddDataFile( outname, actionArgs );
      outputx_ = outputy_ = outputz_ = outputa_ = outputr_;
    }
  }
  // Add DataSets
  dsname_ = actionArgs.GetStringNext();
  if (dsname_.empty())
    dsname_ = init.DSL().GenerateDefaultName("Diff");
  avg_x_ = init.DSL().AddSet(DataSet::DOUBLE, MetaData(dsname_, "X"));
  avg_y_ = init.DSL().AddSet(DataSet::DOUBLE, MetaData(dsname_, "Y"));
  avg_z_ = init.DSL().AddSet(DataSet::DOUBLE, MetaData(dsname_, "Z"));
  avg_r_ = init.DSL().AddSet(DataSet::DOUBLE, MetaData(dsname_, "R"));
  avg_a_ = init.DSL().AddSet(DataSet::DOUBLE, MetaData(dsname_, "A"));
  if (avg_x_ == 0 || avg_y_ == 0 || avg_z_ == 0 || avg_r_ == 0 || avg_a_ == 0)
    return Action::ERR;
  if (outputr_ != 0) outputr_->AddDataSet( avg_r_ );
  if (outputx_ != 0) outputx_->AddDataSet( avg_x_ );
  if (outputy_ != 0) outputy_->AddDataSet( avg_y_ );
  if (outputz_ != 0) outputz_->AddDataSet( avg_z_ );
  if (outputa_ != 0) outputa_->AddDataSet( avg_a_ );
  // Set X dim
  Xdim_ = Dimension(0.0, time_, "Time");
  avg_x_->SetDim(Dimension::X, Xdim_);
  avg_y_->SetDim(Dimension::X, Xdim_);
  avg_z_->SetDim(Dimension::X, Xdim_);
  avg_r_->SetDim(Dimension::X, Xdim_);
  avg_a_->SetDim(Dimension::X, Xdim_);
  // Save master data set list
  masterDSL_ = init.DslPtr();

  mprintf("    DIFFUSION:\n");
  mprintf("\tAtom Mask is [%s]\n", mask_.MaskString());
  if (printIndividual_)
    mprintf("\tBoth average and individual diffusion will be calculated.\n");
  else
    mprintf("\tOnly average diffusion will be calculated.\n");
  mprintf("\tData set base name: %s\n", avg_x_->Meta().Name().c_str());
  // If one file defined, assume all are.
  if (outputx_ != 0) {
    mprintf("\tOutput files:\n"
            "\t  %s: (x) Mean square displacement(s) in the X direction (in Å^2).\n"
            "\t  %s: (y) Mean square displacement(s) in the Y direction (in Å^2).\n"
            "\t  %s: (z) Mean square displacement(s) in the Z direction (in Å^2).\n"
            "\t  %s: (r) Overall mean square displacement(s) (in Å^2).\n"
            "\t  %s: (a) Total distance travelled (in Å).\n",
            outputx_->DataFilename().full(), outputy_->DataFilename().full(),
            outputz_->DataFilename().full(), outputr_->DataFilename().full(),
            outputa_->DataFilename().full());
  }
  mprintf("\tThe time between frames is %g ps.\n", time_);
  mprintf("\tTo calculate diffusion constant from the total mean squared displacement plot,\n"
          "\tcalculate the slope of MSD vs time and multiply by 10.0/6.0; this will give\n"
          "\tunits of 1x10^-5 cm^2/s\n");

  return Action::OK;
}

// Action_Diffusion::Setup()
Action::RetType Action_Diffusion::Setup(ActionSetup& setup) {
  // Setup atom mask
  if (setup.Top().SetupIntegerMask( mask_ )) return Action::ERR;
  mask_.MaskInfo();
  if (mask_.None()) {
    mprintf("Warning: No atoms selected.\n");
    return Action::SKIP;
  }

  // Check for box
  if ( setup.CoordInfo().TrajBox().Type() != Box::NOBOX ) {
    // Currently only works for orthogonal boxes
    if ( setup.CoordInfo().TrajBox().Type() != Box::ORTHO ) {
      mprintf("Warning: Imaging currently only works with orthogonal boxes.\n"
              "Warning:   Imaging will be disabled for this command. This may result in\n"
              "Warning:   large jumps if target molecule is imaged. To counter this please\n"
              "Warning:   use the 'unwrap' command prior to 'diffusion'.\n");
      hasBox_ = false;
    } else 
      hasBox_ = true;
  } else
    hasBox_ = false;

  // Allocate the delta array
  delta_.assign( mask_.Nselected() * 3, 0.0 );

  // Reserve space for the previous coordinates array
  previous_.reserve( mask_.Nselected() * 3 );

  // If initial frame already set and current # atoms > # atoms in initial
  // frame this will probably cause an error.
  if (!initial_.empty() && setup.Top().Natom() > initial_.Natom()) {
    mprintf("Warning: # atoms in current parm (%s, %i) > # atoms in initial frame (%i)\n",
             setup.Top().c_str(), setup.Top().Natom(), initial_.Natom());
    mprintf("Warning: This may lead to segmentation faults.\n");
  }

  // Set up sets for individual atoms if necessary
  if (printIndividual_) {
    for (AtomMask::const_iterator at = mask_.begin(); at != mask_.end(); at++)
    {
      if (*at >= (int)atom_x_.size()) {
        int newSize = *at + 1;
        atom_x_.resize( newSize, 0 );
        atom_y_.resize( newSize, 0 );
        atom_z_.resize( newSize, 0 );
        atom_r_.resize( newSize, 0 );
        atom_a_.resize( newSize, 0 );
      }
      if (atom_x_[*at] == 0) { // TODO: FLOAT?
        atom_x_[*at] = masterDSL_->AddSet(DataSet::DOUBLE, MetaData(dsname_, "aX", *at+1));
        atom_y_[*at] = masterDSL_->AddSet(DataSet::DOUBLE, MetaData(dsname_, "aY", *at+1));
        atom_z_[*at] = masterDSL_->AddSet(DataSet::DOUBLE, MetaData(dsname_, "aZ", *at+1));
        atom_r_[*at] = masterDSL_->AddSet(DataSet::DOUBLE, MetaData(dsname_, "aR", *at+1));
        atom_a_[*at] = masterDSL_->AddSet(DataSet::DOUBLE, MetaData(dsname_, "aA", *at+1));
        if (outputx_ != 0) outputx_->AddDataSet(atom_x_[*at]);
        if (outputy_ != 0) outputy_->AddDataSet(atom_y_[*at]);
        if (outputz_ != 0) outputz_->AddDataSet(atom_z_[*at]);
        if (outputr_ != 0) outputr_->AddDataSet(atom_r_[*at]);
        if (outputa_ != 0) outputa_->AddDataSet(atom_a_[*at]);
        atom_x_[*at]->SetDim(Dimension::X, Xdim_);
        atom_y_[*at]->SetDim(Dimension::X, Xdim_);
        atom_z_[*at]->SetDim(Dimension::X, Xdim_);
        atom_r_[*at]->SetDim(Dimension::X, Xdim_);
        atom_a_[*at]->SetDim(Dimension::X, Xdim_);
      }
    }
  }

  return Action::OK;
}

// Action_Diffusion::DoAction()
Action::RetType Action_Diffusion::DoAction(int frameNum, ActionFrame& frm) {
  // Load initial frame if necessary
  if (initial_.empty()) {
    initial_ = frm.Frm();
    for (AtomMask::const_iterator atom = mask_.begin(); atom != mask_.end(); ++atom)
    {
      const double* XYZ = frm.Frm().XYZ(*atom);
      previous_.push_back( XYZ[0] );
      previous_.push_back( XYZ[1] );
      previous_.push_back( XYZ[2] );
    }
  } else {
    if (hasBox_) 
      boxcenter_ = frm.Frm().BoxCrd().Center();
    Vec3 boxL = frm.Frm().BoxCrd().Lengths();
    // For averaging over selected atoms
    double average2 = 0.0;
    double avgx = 0.0;
    double avgy = 0.0;
    double avgz = 0.0;
    unsigned int idx = 0;
    for (AtomMask::const_iterator at = mask_.begin(); at != mask_.end(); ++at, idx += 3)
    { // Get current and initial coords for this atom.
      const double* XYZ = frm.Frm().XYZ(*at);
      const double* iXYZ = initial_.XYZ(*at);
      // Calculate distance to previous frames coordinates.
      double delx = XYZ[0] - previous_[idx  ];
      double dely = XYZ[1] - previous_[idx+1];
      double delz = XYZ[2] - previous_[idx+2];
      // If the particle moved more than half the box, assume it was imaged
      // and adjust the distance of the total movement with respect to the
      // original frame.
      if (hasBox_) {
        if      (delx >  boxcenter_[0]) delta_[idx  ] -= boxL[0];
        else if (delx < -boxcenter_[0]) delta_[idx  ] += boxL[0];
        else if (dely >  boxcenter_[1]) delta_[idx+1] -= boxL[1];
        else if (dely < -boxcenter_[1]) delta_[idx+1] += boxL[1];
        else if (delz >  boxcenter_[2]) delta_[idx+2] -= boxL[2];
        else if (delz < -boxcenter_[2]) delta_[idx+2] += boxL[2];
      }
      // DEBUG
      //if (debug_ > 2) mprintf("ATOM: %5i %10.3f %10.3f %10.3f",*at,XYZ[0],delx,delta_[idx  ]);
      // Set the current x with reference to the un-imaged trajectory.
      double xx = XYZ[0] + delta_[idx  ]; 
      double yy = XYZ[1] + delta_[idx+1]; 
      double zz = XYZ[2] + delta_[idx+2];
      // Calculate the distance between this "fixed" coordinate
      // and the reference (initial) frame.
      delx = xx - iXYZ[0];
      dely = yy - iXYZ[1];
      delz = zz - iXYZ[2];
      //if (debug_ > 2) mprintf(" %10.3f\n", delx); // DEBUG
      // Calc distances for this atom
      double distx = delx * delx;
      double disty = dely * dely;
      double distz = delz * delz;
      double dist2 = distx + disty + distz;
      // Accumulate averages
      avgx += distx;
      avgy += disty;
      avgz += distz;
      average2 += dist2;
      // Store distances for this atom
      if (printIndividual_) {
        atom_x_[*at]->Add(frameNum, &distx);
        atom_y_[*at]->Add(frameNum, &disty);
        atom_z_[*at]->Add(frameNum, &distz);
        atom_r_[*at]->Add(frameNum, &dist2);
        dist2 = sqrt(dist2);
        atom_a_[*at]->Add(frameNum, &dist2);
      }
      // Update the previous coordinate set to match the current coordinates
      previous_[idx  ] = XYZ[0];
      previous_[idx+1] = XYZ[1];
      previous_[idx+2] = XYZ[2];
    } // END loop over selected atoms
    // Calc averages
    double dNselected = (double)mask_.Nselected();
    avgx /= dNselected;
    avgy /= dNselected;
    avgz /= dNselected;
    average2 /= dNselected;
    // Save averages
    avg_x_->Add(frameNum, &avgx);
    avg_y_->Add(frameNum, &avgy);
    avg_z_->Add(frameNum, &avgz);
    avg_r_->Add(frameNum, &average2);
    average2 = sqrt(average2);
    avg_a_->Add(frameNum, &average2);
  }
  return Action::OK;
}
