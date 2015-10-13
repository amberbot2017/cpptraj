#include "CpptrajState.h"
#include "CpptrajStdio.h"
#include "MpiRoutines.h" // worldrank
#include "Action_CreateCrd.h" // in case default COORDS need to be created
#include "Timer.h"
#include "DataSet_Coords_REF.h" // AddReference
#include "DataSet_Topology.h" // AddTopology
#include "ProgressBar.h"
#ifdef TIMER
#ifdef MPI
#include "EnsembleIn.h"
#endif
#endif

// CpptrajState::AddTrajin()
int CpptrajState::AddTrajin( ArgList& argIn, bool isEnsemble ) {
  std::string fname = argIn.GetStringNext();
  Topology* tempParm = DSL_.GetTopology( argIn );
  if (isEnsemble) {
    if ( trajinList_.AddEnsemble( fname, tempParm, argIn ) ) return 1;
  } else {
    if ( trajinList_.AddTrajin( fname, tempParm, argIn ) ) return 1;
  }
  return 0;
}

// CpptrajState::AddTrajin()
int CpptrajState::AddTrajin( std::string const& fname ) {
  ArgList blank;
  if ( trajinList_.AddTrajin( fname, DSL_.GetTopology(blank), blank ) ) return 1;
  return 0;
}

int CpptrajState::AddOutputTrajectory( ArgList& argIn ) {
  std::string fname = argIn.GetStringNext();
  Topology* top = DSL_.GetTopology( argIn );
  return trajoutList_.AddTrajout( fname, argIn, top );
}

int CpptrajState::AddOutputTrajectory( std::string const& fname ) {
  // FIXME Should this use the last Topology instead?
  ArgList blank;
  return trajoutList_.AddTrajout( fname, blank, DSL_.GetTopology(blank) );
}
// -----------------------------------------------------------------------------
int CpptrajState::WorldSize() { return worldsize; }

CpptrajState::ListKeyType CpptrajState::ListKeys[] = {
  {L_ACTION,   "actions" }, {L_ACTION,   "action"   },
  {L_TRAJIN,   "trajin"  },
  {L_REF,      "ref"     }, {L_REF,      "reference"},
  {L_TRAJOUT,  "trajout" },
  {L_PARM,     "parm"    }, {L_PARM,     "topology" },
  {L_ANALYSIS, "analysis"}, {L_ANALYSIS, "analyses" },
  {L_DATAFILE, "datafile"}, {L_DATAFILE, "datafiles"},
  {L_DATASET,  "dataset" }, {L_DATASET,  "datasets" }, {L_DATASET, "data"},
  {N_LISTS,    0         }
};

std::string CpptrajState::PrintListKeys() {
  std::string keys;
  for (const ListKeyType* ptr = ListKeys; ptr->Key_ != 0; ptr++) {
    keys += " ";
    keys.append( ptr->Key_ );
  }
  return keys;
}

/** Select lists from ArgList */
std::vector<bool> CpptrajState::ListsFromArg( ArgList& argIn, bool allowEmptyKeyword ) const {
  std::vector<bool> enabled( (int)N_LISTS, false );
  std::string listKeyword = argIn.GetStringNext();
  if (listKeyword.empty() || listKeyword == "all") {
    // See if enabling all lists is permissible.
    if (listKeyword.empty() && !allowEmptyKeyword) {
      mprinterr("Error: A specific list name or 'all' must be specified for '%s'\n",
                argIn.Command());
      return enabled; // All are false
    }
    enabled.assign( (int)N_LISTS, true );
  } else {
    while (!listKeyword.empty()) {
      const ListKeyType* ptr = ListKeys;
      for (; ptr->Key_ != 0; ptr++)
        if (listKeyword == ptr->Key_) {
          enabled[ptr->Type_] = true;
          break;
        }
      if (ptr->Key_ == 0) {
        mprinterr("Error: Unrecognized list name: '%s'\n", listKeyword.c_str());
        mprinterr("Error: Recognized keys:%s\n", PrintListKeys().c_str());
        return std::vector<bool>( (int)N_LISTS, false );
      }
      listKeyword = argIn.GetStringNext();
    }
  }
  return enabled;
}

/** List all members of specified lists */
int CpptrajState::ListAll( ArgList& argIn ) const {
  std::vector<bool> enabled = ListsFromArg( argIn, true );
  if ( enabled[L_ACTION]   ) actionList_.List();
  if ( enabled[L_TRAJIN]   ) trajinList_.List();
  if ( enabled[L_REF]      ) DSL_.ListReferenceFrames();
  if ( enabled[L_TRAJOUT]  ) trajoutList_.List( trajinList_.PindexFrames() );
  if ( enabled[L_PARM]     ) DSL_.ListTopologies();
  if ( enabled[L_ANALYSIS] ) analysisList_.List();
  if ( enabled[L_DATAFILE] ) DFL_.List();
  if ( enabled[L_DATASET]  ) DSL_.List();
  return 0;
}

/** Set debug level of specified lists. */
int CpptrajState::SetListDebug( ArgList& argIn ) {
  debug_ = argIn.getNextInteger(0);
  std::vector<bool> enabled = ListsFromArg( argIn, true );
  if ( enabled[L_ACTION]   ) actionList_.SetDebug( debug_ );
  if ( enabled[L_TRAJIN]   ) trajinList_.SetDebug( debug_ );
//  if ( enabled[L_REF]      ) refFrames_.SetDebug( debug_ );
  if ( enabled[L_TRAJOUT]  ) trajoutList_.SetDebug( debug_ );
//  if ( enabled[L_PARM]     ) parmFileList_.SetDebug( debug_ );
  if ( enabled[L_ANALYSIS] ) analysisList_.SetDebug( debug_ );
  if ( enabled[L_DATAFILE] ) DFL_.SetDebug( debug_ );
  if ( enabled[L_DATASET]  ) DSL_.SetDebug( debug_ );
  return 0;
}

/** Clear specified lists */
int CpptrajState::ClearList( ArgList& argIn ) {
  std::vector<bool> enabled = ListsFromArg( argIn, false );
  if ( enabled[L_ACTION]   ) actionList_.Clear();
  if ( enabled[L_TRAJIN]   ) trajinList_.Clear();
//  if ( enabled[L_REF]      ) refFrames_.Clear();
  if ( enabled[L_TRAJOUT]  ) trajoutList_.Clear();
//  if ( enabled[L_PARM]     ) parmFileList_.Clear();
  if ( enabled[L_ANALYSIS] ) analysisList_.Clear();
  if ( enabled[L_DATAFILE] ) DFL_.Clear();
  if ( enabled[L_DATASET]  ) DSL_.Clear();
  return 0;
}

/** Remove DataSet from State */
int CpptrajState::RemoveDataSet( ArgList& argIn ) {
  // Need to first make sure they are removed from DataFiles etc also.
  // FIXME: Currently no good way to check if Actions/Analyses will be
  //        made invalid by DataSet removal.
  std::string removeArg = argIn.GetStringNext();
  if (removeArg.empty()) {
    mprinterr("Error: No data set(s) specified for removal.\n");
    return 1;
  }
  DataSetList tempDSL = DSL_.GetMultipleSets( removeArg );
  if (!tempDSL.empty()) {
    for (DataSetList::const_iterator ds = tempDSL.begin();
                                     ds != tempDSL.end(); ++ds)
    {
      mprintf("\tRemoving \"%s\"\n", (*ds)->legend());
      DFL_.RemoveDataSet( *ds );
      DSL_.RemoveSet( *ds );
    }
  }
  return 0;
}

// CpptrajState::TrajLength()
// NOTE: MMPBSA.py relies on this.
int CpptrajState::TrajLength( std::string const& topname, 
                              std::vector<std::string> const& trajinFiles)
{
  if (AddTopology( topname, ArgList() )) return 1;
  for (std::vector<std::string>::const_iterator trajinName = trajinFiles.begin();
                                                trajinName != trajinFiles.end();
                                                ++trajinName)
    if (AddTrajin( *trajinName )) return 1;
  loudPrintf("Frames: %i\n", trajinList_.MaxFrames());
  return 0;
}

// -----------------------------------------------------------------------------
// CpptrajState::Run()
int CpptrajState::Run() {
  int err = 0;
  // Special case: check if _DEFAULTCRD_ COORDS DataSet is defined. If so,
  // this means 1 or more actions has requested that a default COORDS DataSet
  // be created.
  DataSet* default_crd = DSL_.FindSetOfType("_DEFAULTCRD_", DataSet::COORDS);
  if (default_crd != 0) {
    mprintf("Warning: One or more analyses requested creation of default COORDS DataSet.\n");
    // If the DataSet has already been written to do not create again.
    if (default_crd->Size() > 0)
      mprintf("Warning: Default COORDS DataSet has already been written to.\n");
    else {
      // If no input trajectories this will not work.
      if (trajinList_.empty()) {
        mprinterr("Error: Cannot create COORDS DataSet; no input trajectories specified.\n");
        return 1;
      }
      ArgList crdcmd("createcrd _DEFAULTCRD_");
      crdcmd.MarkArg(0);
      if (AddAction( Action_CreateCrd::Alloc, crdcmd ))
        return 1;
    }
  }
  mprintf("---------- RUN BEGIN -------------------------------------------------\n");
  if (trajinList_.empty()) 
    mprintf("Warning: No input trajectories specified.\n");
  else if (actionList_.Empty() && trajoutList_.Empty())
    mprintf("Warning: No actions/output trajectories specified.\n");
  else {
#   ifdef MPI
    // Only ensemble mode allowed with MPI for now.
    if ( trajinList_.Mode() != TrajinList::ENSEMBLE ) {
      mprinterr("Error: Only 'ensemble' mode supported in parallel.\n");
      err = 1;
    } else
      err = RunEnsemble();
#   else
    switch ( trajinList_.Mode() ) {
      case TrajinList::NORMAL   : err = RunNormal(); break;
      case TrajinList::ENSEMBLE : err = RunEnsemble(); break;
      case TrajinList::UNDEFINED: break;
    }
#   endif
    // Clean up Actions if run completed successfully.
    if (err == 0) {
      actionList_.Clear();
      trajoutList_.Clear();
      DSL_.SetDataSetsPending(false);
    }
  }
  // Run Analyses if any are specified.
  if (err == 0)
    err = RunAnalyses();
  DSL_.List();
  // Print DataFile information and write DataFiles
  DFL_.List();
  MasterDataFileWrite();
  mprintf("---------- RUN END ---------------------------------------------------\n");
  return err;
}

// -----------------------------------------------------------------------------
// CpptrajState::RunEnsemble()
int CpptrajState::RunEnsemble() {
  Timer init_time;
  init_time.Start();
  FrameArray FrameEnsemble;
  FramePtrArray SortedFrames;

  mprintf("\nINPUT ENSEMBLE:\n");
  // Ensure all ensembles are of the same size
  int ensembleSize = -1;
  for (TrajinList::ensemble_it traj = trajinList_.ensemble_begin(); 
                               traj != trajinList_.ensemble_end(); ++traj) 
  {
    if (ensembleSize == -1) {
      ensembleSize = (*traj)->EnsembleCoordInfo().EnsembleSize();
#     ifdef MPI
      // TODO: Eventually try to divide ensemble among MPI threads?
      if (worldsize != ensembleSize) {
        mprinterr("Error: Ensemble size (%i) does not match # of MPI threads (%i).\n",
                  ensembleSize, worldsize);
        return 1;
      }
#     endif
    } else if (ensembleSize != (*traj)->EnsembleCoordInfo().EnsembleSize()) {
      mprinterr("Error: Ensemble size (%i) does not match first ensemble size (%i).\n",
                (*traj)->EnsembleCoordInfo().EnsembleSize(), ensembleSize);
      return 1;
    }
  }
  mprintf("  Ensemble size is %i\n", ensembleSize);
  // Allocate space to hold position of each incoming frame in replica space.
# ifdef MPI
  // Only two frames needed; one for reading, one for receiving.
  SortedFrames.resize( 2 );
  FrameEnsemble.resize( 2 );
# else
  SortedFrames.resize( ensembleSize );
  FrameEnsemble.resize( ensembleSize );
# endif
  //FrameEnsemble.SetupFrames( TrajParm()->Atoms(), cInfo_ );

  // At this point all ensembles should match (i.e. same map etc.)
  trajinList_.FirstEnsembleReplicaInfo();

  // Calculate frame division among trajectories
  trajinList_.List();
  // Parameter file information
  DSL_.ListTopologies();
  // Print reference information 
  DSL_.ListReferenceFrames();
  // Use separate TrajoutList. Existing trajout in current TrajoutList
  // will be converted to ensemble trajout.
  EnsembleOutList ensembleOut;
  if (!trajoutList_.Empty()) {
    if (trajoutList_.MakeEnsembleTrajout(ensembleOut, ensembleSize))
      return 1;
    mprintf("\nENSEMBLE OUTPUT TRAJECTORIES (Numerical filename"
            " suffix corresponds to above map):\n");
    parallel_barrier();
    ensembleOut.List( trajinList_.PindexFrames() );
    parallel_barrier();
  }
  // Allocate DataSets in the master DataSetList based on # frames to be read
  DSL_.AllocateSets( trajinList_.MaxFrames() );
# ifdef MPI
  // Each thread will process one member of the ensemble, so local ensemble
  // size is effectively 1.
  ensembleSize = 1;
# endif
  // Allocate an ActionList for each member of the ensemble.
  std::vector<ActionList*> ActionEnsemble( ensembleSize );
  ActionEnsemble[0] = &actionList_;
  for (int member = 1; member < ensembleSize; member++)
    ActionEnsemble[member] = new ActionList();
  // If we are on a single thread, give each member its own copy of the
  // current topology address. This way if topology is modified by a member,
  // e.g. in strip or closest, subsequent members wont be trying to modify 
  // an already-modified topology.
  std::vector<ActionSetup> EnsembleParm( ensembleSize );
  // Give each member its own copy of current frame address. This way if 
  // frame is modified by a member things like trajout know about it.
  FramePtrArray CurrentFrames( ensembleSize );
# ifdef MPI
  // Make all sets not in an ensemble a member of this thread.
  DSL_.MakeDataSetsEnsemble( worldrank );
  // This tells all DataFiles to append member number.
  DFL_.MakeDataFilesEnsemble( worldrank );
  // Actions have already been set up for this ensemble.
# else
  // Make all sets not in an ensemble part of member 0.
  DSL_.MakeDataSetsEnsemble( 0 );
  // Silence action output for members > 0.
  if (debug_ == 0) SetWorldSilent( true ); 
  // Set up Actions for each ensemble member > 0.
  for (int member = 1; member < ensembleSize; ++member) {
    // All DataSets that will be set up will be part of this ensemble 
    DSL_.SetEnsembleNum( member );
    ActionInit init( DSL_, DFL_ );
    // Initialize actions for this ensemble member based on original actionList_
    if (!actionList_.Empty()) {
      if (debug_ > 0) mprintf("***** ACTIONS FOR ENSEMBLE MEMBER %i:\n", member);
      for (int iaction = 0; iaction < actionList_.Naction(); iaction++) { 
        ArgList actionArgs = actionList_.ActionArgs(iaction);
        // Attempt to add same action to this ensemble. 
        if (ActionEnsemble[member]->AddAction(actionList_.ActionAlloc(iaction), actionArgs, init))
            return 1;
      }
    }
  }
  SetWorldSilent( false );
# endif
  init_time.Stop();
  // Re-enable output
  SetWorldSilent( false );
  mprintf("TIME: Run Initialization took %.4f seconds.\n", init_time.Total()); 
  // ========== A C T I O N  P H A S E ==========
  int lastPindex=-1;          // Index of the last loaded parm file
  int readSets = 0;
  int actionSet = 0;
  bool hasVelocity = false;
# ifdef TIMER
  Timer trajin_time;
  Timer setup_time;
  Timer actions_time;
  Timer trajout_time;
# endif
  Timer frames_time;
  frames_time.Start();
  // Loop over every trajectory in trajFileList
  mprintf("\nBEGIN ENSEMBLE PROCESSING:\n");
  ProgressBar progress;
  if (showProgress_)
    progress.SetupProgress( trajinList_.MaxFrames() );
  for ( TrajinList::ensemble_it traj = trajinList_.ensemble_begin();
                                traj != trajinList_.ensemble_end(); ++traj)
  {
    // Open up the trajectory file. If an error occurs, bail
    if ( (*traj)->BeginEnsemble() ) {
      mprinterr("Error: Could not open trajectory %s.\n",(*traj)->Traj().Filename().full());
      break;
    }
    // Set current parm from current traj.
    Topology* currentParm = (*traj)->Traj().Parm();
    int topFrames = trajinList_.TopFrames( currentParm->Pindex() );
    CoordinateInfo const& currentCoordInfo = (*traj)->EnsembleCoordInfo();
    currentParm->SetBoxFromTraj( currentCoordInfo.TrajBox() ); // FIXME necessary?
    for (int member = 0; member < ensembleSize; ++member)
      EnsembleParm[member].Set( currentParm, currentCoordInfo, topFrames );
    // Check if parm has changed
    bool parmHasChanged = (lastPindex != currentParm->Pindex());
#   ifdef TIMER
    setup_time.Start();
#   endif
    // If Parm has changed or trajectory velocity status has changed,
    // reset the frame.
    if (parmHasChanged || (hasVelocity != currentCoordInfo.HasVel()))
      FrameEnsemble.SetupFrames(currentParm->Atoms(), currentCoordInfo);
    hasVelocity = currentCoordInfo.HasVel();

    // If Parm has changed, reset actions for new topology.
    if (parmHasChanged) {
      // Set up actions for this parm
      bool setupOK = true;
      for (int member = 0; member < ensembleSize; ++member) {
        // Silence action output for all beyond first member.
        if (member > 0)
          SetWorldSilent( true );
        if (ActionEnsemble[member]->SetupActions( EnsembleParm[member] )) {
#         ifdef MPI
          rprintf("Warning: Ensemble member %i: Could not set up actions for %s: skipping.\n",
                  worldrank, EnsembleParm[member].Top().c_str());
#         else
          mprintf("Warning: Ensemble member %i: Could not set up actions for %s: skipping.\n",
                  member, EnsembleParm[member].Top().c_str());
#         endif
          setupOK = false;
        }
      }
      // Re-enable output
      SetWorldSilent( false );
      if (!setupOK) continue;
      // Set up any related output trajectories.
      // TODO: Currently assuming topology is always modified the same
      //       way for all actions. If this behavior ever changes the
      //       following line will cause undesireable behavior.
      ensembleOut.SetupEnsembleOut( EnsembleParm[0].TopAddress(),
                                    EnsembleParm[0].CoordInfo(),
                                    EnsembleParm[0].Nframes() );
      lastPindex = currentParm->Pindex();
    }
#   ifdef TIMER
    setup_time.Stop();
#   endif
    // Loop over every collection of frames in the ensemble
    (*traj)->Traj().PrintInfoLine();
#   ifdef TIMER
    trajin_time.Start();
    bool readMoreFrames = (*traj)->GetNextEnsemble(FrameEnsemble, SortedFrames);
    trajin_time.Stop();
    while ( readMoreFrames )
#   else
    while ( (*traj)->GetNextEnsemble(FrameEnsemble, SortedFrames) )
#   endif
    {
      if (!(*traj)->BadEnsemble()) {
        bool suppress_output = false;
        for (int member = 0; member != ensembleSize; ++member) {
          // Since Frame can be modified by actions, save original and use currentFrame
          ActionFrame currentFrame( SortedFrames[member] );
          //rprintf("DEBUG: currentFrame=%x SortedFrames[0]=%x\n",currentFrame, SortedFrames[0]);
          if ( currentFrame.Frm().CheckCoordsInvalid() )
            rprintf("Warning: Ensemble member %i frame %i may be corrupt.\n",
                    member, (*traj)->Traj().Counter().PreviousFrameNumber()+1);
#         ifdef TIMER
          actions_time.Start();
#         endif
          // Perform Actions on Frame
          suppress_output = ActionEnsemble[member]->DoActions(actionSet, currentFrame);
          CurrentFrames[member] = currentFrame.FramePtr();
#         ifdef TIMER
          actions_time.Stop();
#         endif
        } // END loop over actions
        // Do Output
        if (!suppress_output) {
#         ifdef TIMER
          trajout_time.Start();
#         endif 
          if (ensembleOut.WriteEnsembleOut(actionSet, CurrentFrames))
          {
            mprinterr("Error: Writing ensemble output traj, frame %i\n", actionSet+1);
            if (exitOnError_) return 1; 
          }
#         ifdef TIMER
          trajout_time.Stop();
#         endif
        }
      } else {
#       ifdef MPI
        rprinterr("Error: Could not read frame %i for ensemble.\n", actionSet + 1);
#       else
        mprinterr("Error: Could not read frame %i for ensemble.\n", actionSet + 1);
#       endif
      }
      if (showProgress_) progress.Update( actionSet );
      // Increment frame counter
      ++actionSet;
#     ifdef TIMER
      trajin_time.Start();
      readMoreFrames = (*traj)->GetNextEnsemble(FrameEnsemble, SortedFrames);
      trajin_time.Stop();
#     endif
    }

    // Close the trajectory file
    (*traj)->EndEnsemble();
    // Update how many frames have been processed.
    readSets += (*traj)->Traj().Counter().NumFramesProcessed();
    mprintf("\n");
  } // End loop over trajin
  mprintf("Read %i frames and processed %i frames.\n",readSets,actionSet);
  frames_time.Stop();
  frames_time.WriteTiming(0," Trajectory processing:");
  mprintf("TIME: Avg. throughput= %.4f frames / second.\n",
          (double)readSets / frames_time.Total());
# ifdef TIMER
  trajin_time.WriteTiming(1,  "Trajectory read:        ", frames_time.Total());
  setup_time.WriteTiming(1,   "Action setup:           ", frames_time.Total());
  actions_time.WriteTiming(1, "Action frame processing:", frames_time.Total());
  trajout_time.WriteTiming(1, "Trajectory output:      ", frames_time.Total());
# ifdef MPI
  Ensemble::TimingData(trajin_time.Total());
# endif
# endif

  // Close output trajectories
  ensembleOut.CloseEnsembleOut();

  // ========== A C T I O N  O U T P U T  P H A S E ==========
  mprintf("\nENSEMBLE ACTION OUTPUT:\n");
  for (int member = 0; member < ensembleSize; ++member)
    ActionEnsemble[member]->Print( );
# ifdef MPI
  // Sync DataSets across all threads. 
  //DSL_.SynchronizeData(); // NOTE: Disabled, trajs are not currently divided.
# endif
  // Clean up ensemble action lists
  for (int member = 1; member < ensembleSize; member++)
    delete ActionEnsemble[member];

  return 0;
}

// -----------------------------------------------------------------------------
// CpptrajState::RunNormal()
/** Process trajectories in trajinList. Each frame in trajinList is sent
 *  to the actions in actionList for processing.
 */
int CpptrajState::RunNormal() {
  int actionSet=0;            // Internal data frame
  int readSets=0;             // Number of frames actually read
  int lastPindex=-1;          // Index of the last loaded parm file
  Frame TrajFrame;            // Original Frame read in from traj

  // ========== S E T U P   P H A S E ========== 
  Timer init_time;
  init_time.Start();
  // Parameter file information
  DSL_.ListTopologies();
  // Input coordinate file information
  trajinList_.List();
  // Print reference information
  DSL_.ListReferenceFrames(); 
  // Output traj
  trajoutList_.List( trajinList_.PindexFrames() );
  // Allocate DataSets in the master DataSetList based on # frames to be read
  DSL_.AllocateSets( trajinList_.MaxFrames() );
  init_time.Stop();
  mprintf("TIME: Run Initialization took %.4f seconds.\n", init_time.Total());
  
  // ========== A C T I O N  P H A S E ==========
  // Loop over every trajectory in trajFileList
# ifdef TIMER
  Timer trajin_time;
  Timer setup_time;
  Timer actions_time;
  Timer trajout_time;
# endif
  Timer frames_time;
  frames_time.Start();
  mprintf("\nBEGIN TRAJECTORY PROCESSING:\n");
  ProgressBar progress;
  if (showProgress_)
    progress.SetupProgress( trajinList_.MaxFrames() );
  for ( TrajinList::trajin_it traj = trajinList_.trajin_begin();
                              traj != trajinList_.trajin_end(); ++traj)
  {
    // Open up the trajectory file. If an error occurs, bail 
    if ( (*traj)->BeginTraj() ) {
      mprinterr("Error: Could not open trajectory %s.\n",(*traj)->Traj().Filename().full());
      break;
    }
    // Set current parm from current traj.
    Topology* top = (*traj)->Traj().Parm();
    top->SetBoxFromTraj( (*traj)->TrajCoordInfo().TrajBox() ); // FIXME necessary?
    int topFrames = trajinList_.TopFrames( top->Pindex() );
    ActionSetup currentParm( top, (*traj)->TrajCoordInfo(), topFrames );
    // Check if parm has changed
    bool parmHasChanged = (lastPindex != currentParm.Top().Pindex());
#   ifdef TIMER
    setup_time.Start();
#   endif
    // If Parm has changed or trajectory frame has changed, reset the frame.
    if (parmHasChanged || 
        (TrajFrame.HasVelocity() != currentParm.CoordInfo().HasVel()) ||
        ((int)TrajFrame.RemdIndices().size() !=
              currentParm.CoordInfo().ReplicaDimensions().Ndims()))
      TrajFrame.SetupFrameV(currentParm.Top().Atoms(), currentParm.CoordInfo());
    // If Parm has changed, reset actions for new topology.
    if (parmHasChanged) {
      // Set up actions for this parm
      if (actionList_.SetupActions( currentParm )) {
        mprintf("WARNING: Could not set up actions for %s: skipping.\n",
                currentParm.Top().c_str());
        continue;
      }
      // Set up any related output trajectories 
      trajoutList_.SetupTrajout( currentParm.TopAddress(),
                                 currentParm.CoordInfo(),
                                 currentParm.Nframes() );
      lastPindex = currentParm.Top().Pindex();
    }
#   ifdef TIMER
    setup_time.Stop();
#   endif
    // Loop over every Frame in trajectory
    (*traj)->Traj().PrintInfoLine();
#   ifdef TIMER
    trajin_time.Start();
    bool readMoreFrames = (*traj)->GetNextFrame(TrajFrame);
    trajin_time.Stop();
    while ( readMoreFrames )
#   else
    while ( (*traj)->GetNextFrame(TrajFrame) )
#   endif
    {
        // Since Frame can be modified by actions, save original and use currentFrame
      ActionFrame currentFrame( &TrajFrame );
      // Check that coords are valid.
      if ( currentFrame.Frm().CheckCoordsInvalid() )
        mprintf("Warning: Frame %i coords 1 & 2 overlap at origin; may be corrupt.\n",
                (*traj)->Traj().Counter().PreviousFrameNumber()+1);
        // Perform Actions on Frame
#       ifdef TIMER
        actions_time.Start();
#       endif
        bool suppress_output = actionList_.DoActions(actionSet, currentFrame);
#       ifdef TIMER
        actions_time.Stop();
#       endif
        // Do Output
        if (!suppress_output) {
#         ifdef TIMER
          trajout_time.Start();
#         endif
          if (trajoutList_.WriteTrajout(actionSet, currentFrame.Frm())) {
            if (exitOnError_) return 1;
          }
#         ifdef TIMER
          trajout_time.Stop();
#         endif
        }
      if (showProgress_) progress.Update( actionSet );
      // Increment frame counter
      ++actionSet;
#     ifdef TIMER
      trajin_time.Start();
      readMoreFrames = (*traj)->GetNextFrame(TrajFrame);
      trajin_time.Stop();
#     endif 
    }

    // Close the trajectory file
    (*traj)->EndTraj();
    // Update how many frames have been processed.
    readSets += (*traj)->Traj().Counter().NumFramesProcessed();
    mprintf("\n");
  } // End loop over trajin
  mprintf("Read %i frames and processed %i frames.\n",readSets,actionSet);
  frames_time.Stop();
  frames_time.WriteTiming(0," Trajectory processing:");
  mprintf("TIME: Avg. throughput= %.4f frames / second.\n", 
          (double)readSets / frames_time.Total());
# ifdef TIMER
  trajin_time.WriteTiming(1,  "Trajectory read:        ", frames_time.Total());
  setup_time.WriteTiming(1,   "Action setup:           ", frames_time.Total());
  actions_time.WriteTiming(1, "Action frame processing:", frames_time.Total());
  trajout_time.WriteTiming(1, "Trajectory output:      ", frames_time.Total());
# endif
  // Close output trajectories.
  trajoutList_.CloseTrajout();

  // ========== A C T I O N  O U T P U T  P H A S E ==========
  mprintf("\nACTION OUTPUT:\n");
  actionList_.Print( );
# ifdef MPI
  // Sync DataSets across all threads. 
  //DSL_.SynchronizeData(); // NOTE: Disabled, trajs are not currently divided.
# endif

  return 0;
}

// -----------------------------------------------------------------------------
// CpptrajState::MasterDataFileWrite()
// FIXME: If MPI ever used for trajin mode this may have to be protected.
/** Trigger write of all pending DataFiles. When in parallel ensemble mode,
  * each member of the ensemble will write data to separate files with 
  * numeric extensions.
  */
void CpptrajState::MasterDataFileWrite() { DFL_.WriteAllDF(); }

// CpptrajState::RunAnalyses()
int CpptrajState::RunAnalyses() {
  if (analysisList_.Empty()) return 0;
  Timer analysis_time;
  analysis_time.Start();
  int err = analysisList_.DoAnalyses();
  analysis_time.Stop();
  mprintf("TIME: Analyses took %.4f seconds.\n", analysis_time.Total());
  // If all Analyses completed successfully, clean up analyses.
  if ( err == 0) 
    analysisList_.Clear();
  return err;
}

// CpptrajState::AddReference()
/** Add specified file/COORDS set as reference. Reference frames are a unique
  * DataSet - they are set up OUTSIDE data set list.
  */
int CpptrajState::AddReference( std::string const& fname, ArgList const& args ) {
  if (fname.empty()) return 1;
  ArgList argIn = args;
  // 'average' keyword is deprecated
  if ( argIn.hasKey("average") ) {
    mprinterr("Error: 'average' for reference is deprecated. Please use\n"
              "Error:   the 'average' action to create averaged coordinates.\n");
    return 1;
  }
  Topology* refParm = 0;
  DataSet_Coords* CRD = 0;
  if (argIn.hasKey("crdset")) {
    CRD = (DataSet_Coords*)DSL_.FindCoordsSet( fname );
    if (CRD == 0) {
      mprinterr("COORDS set with name %s not found.\n", fname.c_str());
      return 1;
    }
  } else {
    // Get topology file.
    refParm = DSL_.GetTopology( argIn );
    if (refParm == 0) {
      mprinterr("Error: Cannot get topology for reference '%s'\n", fname.c_str());
      return 1;
    }
  }
  // Determine if there is a mask expression for stripping reference. // TODO: Remove?
  std::string maskexpr = argIn.GetMaskNext();
  // Check for tag. FIXME: need to do after SetupTrajRead?
  std::string tag = argIn.getNextTag();
  // Set up reference DataSet from file or COORDS set.
  DataSet_Coords_REF* ref = new DataSet_Coords_REF();
  if (ref==0) return 1;
  if (refParm != 0) {
    if (ref->LoadRefFromFile(fname, tag, *refParm, argIn, debug_)) return 1;
  } else { // CRD != 0
    int fnum;
    if (argIn.hasKey("lastframe"))
      fnum = (int)CRD->Size()-1;
    else
      fnum = argIn.getNextInteger(1) - 1;
    mprintf("\tSetting up reference from COORDS set '%s', frame %i\n",
            CRD->legend(), fnum+1);
    if (ref->SetRefFromCoords(CRD, tag, fnum)) return 1;
  }
  // If a mask expression was specified, strip to match the expression.
  if (!maskexpr.empty()) {
    if (ref->StripRef( maskexpr )) return 1;
  }
  // Add DataSet to main DataSetList.
  if (DSL_.AddSet( ref )) return 1; 
  return 0;
}

// CpptrajState::AddTopology()
/** Add specified file(s) as Topology. Topologies are a unique
  * DataSet - they are set up OUTSIDE data set list.
  */
int CpptrajState::AddTopology( std::string const& fnameIn, ArgList const& args ) {
  if (fnameIn.empty()) return 1;
  File::NameArray fnames = File::ExpandToFilenames( fnameIn );
  if (fnames.empty()) {
    mprinterr("Error: '%s' corresponds to no files.\n");
    return 1;
  }
  ArgList argIn = args;
  // Determine if there is a mask expression for stripping. // TODO: Remove?
  std::string maskexpr = argIn.GetMaskNext();
  // Check for tag.
  std::string tag = argIn.getNextTag();
  for (File::NameArray::const_iterator fname = fnames.begin(); fname != fnames.end(); ++fname)
  {
    MetaData md(*fname, tag, -1);
    DataSet* set = DSL_.CheckForSet( md );
    if (set != 0)
      mprintf("Warning: Set '%s' already present.\n", set->legend());
    else {
      // Create Topology DataSet
      DataSet_Topology* ds = (DataSet_Topology*)DSL_.AddSet(DataSet::TOPOLOGY, md);
      if (ds == 0) { 
        if (exitOnError_) return 1;
      } else {
        if (ds->LoadTopFromFile(argIn, debug_)) {
          DSL_.RemoveSet( ds );
          if (exitOnError_) return 1;
        }
        // If a mask expression was specified, strip to match the expression.
        if (!maskexpr.empty()) {
          if (ds->StripTop( maskexpr )) return 1;
        }
      }
    }
    // TODO: Set active top?
  }
  return 0;
}

int CpptrajState::AddTopology( Topology const& top, std::string const& parmname ) {
  
  DataSet_Topology* ds = (DataSet_Topology*)DSL_.AddSet(DataSet::TOPOLOGY, parmname);
  if (ds == 0) return 1;
  ds->SetTop( top );
  return 0;
}
