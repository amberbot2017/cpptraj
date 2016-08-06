#ifdef BINTRAJ
// This file contains a collection of routines designed for reading
// (and writing?) netcdf restart files used with amber.
// Dan Roe 2011-01-07
#include "Traj_AmberRestartNC.h"
#include <netcdf.h>
#include "NC_Routines.h"
#include "CpptrajStdio.h"
#include "StringRoutines.h" // integerToString 

// CONSTRUCTOR
Traj_AmberRestartNC::Traj_AmberRestartNC() :
  restartTime_(0),
  time0_(1.0),
  dt_(1.0),
  n_atoms_(0),
  singleWrite_(false),
  useVelAsCoords_(false),
  useFrcAsCoords_(false),
  outputTemp_(false),
  readAccess_(false),
  prependExt_(false)
{}

// DESTRUCTOR
Traj_AmberRestartNC::~Traj_AmberRestartNC() {
  //fprintf(stderr,"Amber Netcdf Restart Destructor\n");
  // NOTE: Need to close file?
}

bool Traj_AmberRestartNC::ID_TrajFormat(CpptrajFile& fileIn) {
  if ( GetNetcdfConventions( fileIn.Filename().full() ) == NC_AMBERRESTART ) return true;
  return false;
}

// Traj_AmberRestartNC::closeTraj()
void Traj_AmberRestartNC::closeTraj() {
  NC_close();
}

// Traj_AmberRestartNC::openTrajin()
int Traj_AmberRestartNC::openTrajin() {
  // If already open, return
  if (Ncid()!=-1) return 0;
  if ( NC_openRead( filename_.Full() ) != 0 ) {
    mprinterr("Error: Opening Netcdf restart file %s for reading.\n", filename_.base());
    return 1;
  }
  if (debug_>1) NetcdfDebug();
  return 0;
}

void Traj_AmberRestartNC::ReadHelp() {
  mprintf("\tusevelascoords: Use velocities instead of coordinates if present.\n"
          "\tusefrcascoords: Use forces instead of coordinates if present.\n");
}

int Traj_AmberRestartNC::processReadArgs(ArgList& argIn) {
  useVelAsCoords_ = argIn.hasKey("usevelascoords");
  useFrcAsCoords_ = argIn.hasKey("usefrcascoords");
  return 0;
}

// Traj_AmberRestartNC::setupTrajin()
/** Set up netcdf restart file for reading, get all variable and dimension IDs. 
  * Also check number of atoms against associated parmtop.
  */
int Traj_AmberRestartNC::setupTrajin(FileName const& fname, Topology* trajParm)
{
  filename_ = fname;
  if (openTrajin()) return TRAJIN_ERR;
  readAccess_ = true;
  // Sanity check - Make sure this is a Netcdf restart
  if ( GetNetcdfConventions() != NC_AMBERRESTART ) {
    mprinterr("Error: Netcdf restart file %s conventions do not include \"AMBERRESTART\"\n",
              filename_.base());
    return TRAJIN_ERR;
  }
  // This will warn if conventions are not 1.0 
  CheckConventionsVersion();
  // Get title
  SetTitle( GetNcTitle() );
  // Setup Coordinates/Velocities
  if ( SetupCoordsVelo( useVelAsCoords_, useFrcAsCoords_ )!=0 ) return TRAJIN_ERR;
  // Check that specified number of atoms matches expected number.
  if (Ncatom() != trajParm->Natom()) {
    mprinterr("Error: Number of atoms in NetCDF restart file %s (%i) does not\n",
              filename_.base(), Ncatom());
    mprinterr("       match number in associated parmtop (%i)!\n",trajParm->Natom());
    return TRAJIN_ERR;
  }
  // Setup Time - FIXME: allowed to fail silently
  SetupTime();
  // Box info
  Box nc_box;
  if (SetupBox(nc_box, NC_AMBERRESTART) == 1) // 1 indicates an error
    return TRAJIN_ERR;
  // Replica Temperatures - FIXME: allowed to fail silently 
  SetupTemperature();
  // Replica Dimensions
  ReplicaDimArray remdDim;
  if ( SetupMultiD(remdDim) == -1 ) return TRAJIN_ERR;
  // Set traj info: FIXME - no forces yet
  SetCoordInfo( CoordinateInfo(remdDim, nc_box, HasVelocities(),
                               HasTemperatures(), HasTimes(), false) );
  // NOTE: TO BE ADDED
  // labelDID;
  //int cell_spatialDID, cell_angularDID;
  //int spatialVID, cell_spatialVID, cell_angularVID;
  closeTraj();
  // Only 1 frame for NC restarts
  return 1;
}

void Traj_AmberRestartNC::WriteHelp() {
  mprintf("\tremdtraj: Write temperature to restart file.\n"
          "\ttime0   : Time for first frame (default 1.0).\n"
          "\tdt      : Time step for subsequent frames, t=(time0+frame)*dt; (default 1.0)\n"
          "\tkeepext : Keep filename extension; write '<name>.<num>.<ext>' instead.\n");
}

// Traj_AmberRestartNC::processWriteArgs()
int Traj_AmberRestartNC::processWriteArgs(ArgList& argIn) {
  outputTemp_ = argIn.hasKey("remdtraj");
  prependExt_ = argIn.hasKey("keepext");
  time0_ = argIn.getKeyDouble("time0", -1.0);
  dt_ = argIn.getKeyDouble("dt",1.0);
  return 0;
}

// Traj_AmberRestartNC::setupTrajout()
/** Setting up is done for each frame.  */
int Traj_AmberRestartNC::setupTrajout(FileName const& fname, Topology* trajParm,
                                      CoordinateInfo const& cInfoIn,
                                      int NframesToWrite, bool append)
{
  if (append) {
    mprinterr("Error: 'append' not supported by NetCDF restart\n");
    return 1;
  }
  readAccess_ = false;
  CoordinateInfo cInfo = cInfoIn;
  if (!cInfo.HasTemp() && outputTemp_) cInfo.SetTemperature(true);
  if (!cInfo.HasTime() && time0_ >= 0) cInfo.SetTime(true);
  SetCoordInfo( cInfo );
  filename_ = fname;
  n_atoms_ = trajParm->Natom();
  // If number of frames to write == 1 set singleWrite so we dont append
  // frame # to filename.
  if (NframesToWrite == 1) singleWrite_ = true;
  // Set up title
  if (Title().empty())
    SetTitle("Cpptraj Generated Restart");
  return 0;
}

// Traj_AmberRestartNC::readFrame()
/** Get the specified frame from amber netcdf file
  * Coords are a 1 dimensional array of format X1,Y1,Z1,X2,Y2,Z2,...
  */
int Traj_AmberRestartNC::readFrame(int set, Frame& frameIn) {
  // Get time
  if (timeVID_ != -1) {
    if ( NC::CheckErr(nc_get_var_double(ncid_, timeVID_, frameIn.mAddress())) ) {
      mprinterr("Error: Getting restart time.\n");
      return 1;
    }
  }

  // Get temperature
  if (TempVID_!=-1) {
    if ( NC::CheckErr(nc_get_var_double(ncid_, TempVID_, frameIn.tAddress())) ) {
      mprinterr("Error: Getting replica temperature.\n");
      return 1;
    }
    if (debug_ > 1)
      mprintf("DEBUG: %s: Replica Temperature %lf\n",filename_.base(), frameIn.Temperature());
  }

  // Read Coords 
  start_[0] = 0;
  start_[1] = 0;
  count_[0] = Ncatom();
  count_[1] = 3;
  if ( NC::CheckErr(nc_get_vara_double(ncid_, coordVID_, start_, count_, frameIn.xAddress())) ) {
    mprinterr("Error: Getting Coords\n");
    return 1;
  }

  // Read Velocity
  if (velocityVID_!=-1 && frameIn.HasVelocity()) {
    if ( NC::CheckErr(nc_get_vara_double(ncid_, velocityVID_, start_, count_, frameIn.vAddress())) ) {
      mprinterr("Error: Getting velocities\n"); 
      return 1;
    }
  }

  // Read replica indices
  if (indicesVID_!=-1) {
    count_[0] = remd_dimension_;
    if ( NC::CheckErr(nc_get_vara_int(ncid_, indicesVID_, start_, count_, frameIn.iAddress())) ) {
      mprinterr("Error: Getting replica indices from restart.\n");
      return 1;
    }
    //mprintf("DEBUG:\tReplica Rst indices:");
    //for (int dim=0; dim < remd_dimension_; dim++) mprintf(" %i",remd_indices[dim]);
    //mprintf("\n");
  }

  // Read box info 
  if (cellLengthVID_ != -1) {
    count_[0] = 3;
    count_[1] = 0;
    if ( NC::CheckErr(nc_get_vara_double(ncid_, cellLengthVID_, start_, count_, frameIn.bAddress())) ) {
      mprinterr("Error: Getting cell lengths.\n"); 
      return 1;
    }
    if ( NC::CheckErr(nc_get_vara_double(ncid_, cellAngleVID_, start_, count_, frameIn.bAddress()+3)) ) {
      mprinterr("Error: Getting cell angles.\n");
      return 1;
    }
  }

  return 0;
}

// Traj_AmberRestartNC::writeFrame() 
int Traj_AmberRestartNC::writeFrame(int set, Frame const& frameOut) {
  // Set up file for this set
  bool V_present = (CoordInfo().HasVel() && frameOut.HasVelocity());
  FileName fname;
  // Create filename for this set
  // If just writing 1 frame dont modify output filename
  if (singleWrite_)
    fname = filename_;
  else if (prependExt_)
    fname = filename_.PrependExt( "." + integerToString(set+1) );
  else
    fname = filename_.AppendFileName( "." + integerToString(set+1) );
  // Create Netcdf file 
  if ( NC_create( fname.full(), NC_AMBERRESTART, n_atoms_, CoordInfo(), Title() ) )
    return 1;
  // write coords
  start_[0] = 0;
  start_[1] = 0;
  count_[0] = Ncatom(); 
  count_[1] = 3;
  if (NC::CheckErr(nc_put_vara_double(ncid_,coordVID_,start_,count_,frameOut.xAddress())) ) {
    mprinterr("Error: Netcdf restart Writing coordinates %i\n",set);
    return 1;
  }
  // write velocity
  if (V_present) {
    //mprintf("DEBUG: Writing V, VID=%i\n",velocityVID_);
    if (NC::CheckErr(nc_put_vara_double(ncid_,velocityVID_,start_,count_,frameOut.vAddress())) ) {
      mprinterr("Error: Netcdf restart writing velocity %i\n",set);
      return 1;
    }
  }
  // write box
  if (cellLengthVID_ != -1) {
    count_[0] = 3;
    count_[1] = 0;
    if (NC::CheckErr(nc_put_vara_double(ncid_,cellLengthVID_,start_,count_,frameOut.bAddress())) ) {
      mprinterr("Error: Writing cell lengths.\n");
      return 1;
    }
    if (NC::CheckErr(nc_put_vara_double(ncid_,cellAngleVID_,start_,count_, frameOut.bAddress()+3)) ) {
      mprinterr("Error: Writing cell angles.\n");
      return 1;
    }
  }
  // write time
  if (timeVID_ != -1) {
    if (time0_ >= 0)
      restartTime_ = (time0_ + (double)set) * dt_;
    else
      restartTime_ = frameOut.Time();
    if (NC::CheckErr(nc_put_var_double(ncid_,timeVID_,&restartTime_)) ) {
      mprinterr("Error: Writing restart time.\n");
      return 1;
    }
  }
  // write temperature
  if (TempVID_ != -1) {
    if (NC::CheckErr(nc_put_var_double(ncid_,TempVID_,frameOut.tAddress())) ) {
      mprinterr("Error: Writing restart temperature.\n"); 
      return 1;
    }
  }
  // write indices
  if (indicesVID_ != -1) {
    count_[0] = remd_dimension_;
    if ( NC::CheckErr(nc_put_vara_int(ncid_,indicesVID_,start_,count_,frameOut.iAddress())) ) {
      mprinterr("Error: Writing indices frame %i.\n", set+1);
      return 1;
    }
  }
  //nc_sync(ncid_); // Necessary? File about to close anyway... 
  // Close file for this set
  closeTraj();
  return 0;
}  

// Traj_AmberRestartNC::Info()
void Traj_AmberRestartNC::Info() {
  mprintf("is a NetCDF AMBER restart file");
  if (readAccess_) {
    if (CoordInfo().HasVel()) mprintf(", with velocities");
    if (CoordInfo().HasTemp()) mprintf(", with replica temperature");
    if (remd_dimension_ > 0) mprintf(", with %i dimensions", remd_dimension_);
  } else {
    if (outputTemp_) mprintf(", with temperature");
  }
}
#ifdef MPI
/// Since files are opened on write this does not need to do anything
int Traj_AmberRestartNC::parallelOpenTrajout(Parallel::Comm const& commIn) { return 0; }

/** No file access during setupTrajout, so have all threads call it.
  * No need to sync.
  */
int Traj_AmberRestartNC::parallelSetupTrajout(FileName const& fname, Topology* trajParm,
                                             CoordinateInfo const& cInfoIn,
                                             int NframesToWrite, bool append,
                                             Parallel::Comm const& commIn)
{
  return (setupTrajout(fname, trajParm, cInfoIn, NframesToWrite, append));
}

int Traj_AmberRestartNC::parallelWriteFrame(int set, Frame const& frameOut) {
  return ( writeFrame(set, frameOut) );
}
#endif
#endif
