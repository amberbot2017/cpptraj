#include "Exec_DataSetCmd.h"
#include "CpptrajStdio.h"
#include "DataSet_1D.h"
#include "DataSet_MatrixDbl.h"

void Exec_DataSetCmd::Help() const {
  mprintf("\t{ legend <legend> <set> |\n"
          "\t  makexy <Xset> <Yset> [name <name>] |\n"
          "\t  cat <set0> <set1> ... [name <name>] [nooffset] |\n"
          "\t  make2d <1D set> cols <ncols> rows <nrows> [name <name>]\n"
          "\t  [mode <mode>] [type <type>] <set arg1> [<set arg 2> ...] }\n"
          "\t<mode>: ");
  for (int i = 0; i != (int)MetaData::UNKNOWN_MODE; i++)
    mprintf(" '%s'", MetaData::ModeString((MetaData::scalarMode)i));
  mprintf("\n\t<type>: ");
  for (int i = 0; i != (int)MetaData::UNDEFINED; i++)
    mprintf(" '%s'", MetaData::TypeString((MetaData::scalarType)i));
  mprintf("\n\tOptions for 'type noe':\n"
          "\t  %s\n"
          "  legend: Set the legend for a single data set\n"
          "  makexy: Create new data set with X values from one set and Y values from another.\n"
          "  cat   : Concatenate 2 or more data sets.\n"
          "  make2d: Create new 2D data set from 1D data set, assumes row-major ordering.\n"
          "  Otherwise, change the mode/type for one or more data sets.\n",
          AssociatedData_NOE::HelpText);
}

Exec::RetType Exec_DataSetCmd::Execute(CpptrajState& State, ArgList& argIn) {
  if (argIn.Contains("legend")) { // Set legend for one data set
    std::string legend = argIn.GetStringKey("legend");
    DataSet* ds = State.DSL().GetDataSet( argIn.GetStringNext() );
    if (ds == 0) return CpptrajState::ERR;
    mprintf("\tChanging legend '%s' to '%s'\n", ds->legend(), legend.c_str());
    ds->SetLegend( legend );
  // ---------------------------------------------
  } else if (argIn.hasKey("makexy")) { // Combine values from two sets into 1
    std::string name = argIn.GetStringKey("name");
    DataSet* ds1 = State.DSL().GetDataSet( argIn.GetStringNext() );
    DataSet* ds2 = State.DSL().GetDataSet( argIn.GetStringNext() );
    if (ds1 == 0 || ds2 == 0) return CpptrajState::ERR;
    if (ds1->Ndim() != 1 || ds2->Ndim() != 1) {
      mprinterr("Error: makexy only works for 1D data sets.\n");
      return CpptrajState::ERR;
    }
    DataSet* ds3 = State.DSL().AddSet( DataSet::XYMESH, name, "XY" );
    if (ds3 == 0) return CpptrajState::ERR;
    mprintf("\tUsing values from '%s' as X, values from '%s' as Y, output set '%s'\n",
            ds1->legend(), ds2->legend(), ds3->legend());
    DataSet_1D const& ds_x = static_cast<DataSet_1D const&>( *ds1 );
    DataSet_1D const& ds_y = static_cast<DataSet_1D const&>( *ds2 );
    DataSet_1D&       out  = static_cast<DataSet_1D&>( *ds3 );
    size_t nframes = std::min( ds_x.Size(), ds_y.Size() );
    if (ds_x.Size() != ds_y.Size())
      mprintf("Warning: Data sets do not have equal sizes, only using %zu frames.\n", nframes);
    double XY[2];
    for (size_t i = 0; i != nframes; i++) {
      XY[0] = ds_x.Dval(i);
      XY[1] = ds_y.Dval(i);
      out.Add( i, XY );
    }
  // ---------------------------------------------
  } else if (argIn.hasKey("make2d")) { // Create 2D matrix from 1D set
    std::string name = argIn.GetStringKey("name");
    int ncols = argIn.getKeyInt("ncols", 0);
    int nrows = argIn.getKeyInt("nrows", 0);
    if (ncols < 0 || nrows < 0) {
      mprinterr("Error: Must specify both ncols and nrows\n");
      return CpptrajState::ERR;
    }
    DataSet* ds1 = State.DSL().GetDataSet( argIn.GetStringNext() );
    if (ds1 == 0) return CpptrajState::ERR;
    if (ds1->Ndim() != 1) {
      mprinterr("Error: make2d only works for 1D data sets.\n");
      return CpptrajState::ERR;
    }
    if (nrows * ncols != (int)ds1->Size()) {
      mprinterr("Error: Size of '%s' (%zu) != nrows X ncols.\n", ds1->legend(), ds1->Size());
      return CpptrajState::ERR;
    }
    DataSet* ds3 = State.DSL().AddSet( DataSet::MATRIX_DBL, name, "make2d" );
    if (ds3 == 0) return CpptrajState::ERR;
    mprintf("\tConverting values from 1D set '%s' to 2D matrix '%s' with %i cols, %i rows.\n",
            ds1->legend(), ds3->legend(), ncols, nrows);
    DataSet_1D const& data = static_cast<DataSet_1D const&>( *ds1 );
    DataSet_MatrixDbl& matrix = static_cast<DataSet_MatrixDbl&>( *ds3 );
    if (matrix.Allocate2D( ncols, nrows )) return CpptrajState::ERR;
    for (unsigned int idx = 0; idx != data.Size(); idx++)
      matrix.AddElement( data.Dval(idx) );
  // ---------------------------------------------
  } else if (argIn.hasKey("filter")) { // Filter points in data set to make new data set
    std::string name = argIn.GetStringKey("name");
    int rowmin = argIn.getKeyInt("rowmin", -1);
    int rowmax = argIn.getKeyInt("rowmax", -1);
    int colmin = argIn.getKeyInt("colmin", -1);
    int colmax = argIn.getKeyInt("colmax", -1);
    
    DataSet* ds1 = State.DSL().GetDataSet( argIn.GetStringNext() );
    if (ds1 == 0) return CpptrajState::ERR;
    if ( ds1->Group() == DataSet::SCALAR_1D ) {
      mprinterr("Error: Not yet set up for 1D sets.\n");
      return CpptrajState::ERR;
    } else if (ds1->Group() == DataSet::MATRIX_2D) {
      DataSet_2D const& matrixIn = static_cast<DataSet_2D const&>( *ds1 );
      if (rowmin < 0) rowmin = 0;
      if (rowmax < 0) rowmax = matrixIn.Nrows();
      int nrows = rowmax - rowmin;
      if (nrows < 1) {
        mprinterr("Error: Keeping less than 1 row.\n");
        return CpptrajState::ERR;
      } else if (nrows > (int)matrixIn.Nrows())
        nrows = matrixIn.Nrows();
      if (colmin < 0) colmin = 0;
      if (colmax < 0) colmax = matrixIn.Ncols();
      int ncols = colmax - colmin;
      if (ncols < 1) {
        mprinterr("Error: Keeping less than 1 column.\n");
        return CpptrajState::ERR;
      } else if (ncols > (int)matrixIn.Ncols())
        ncols = matrixIn.Ncols();
      mprintf("\tMatrix to filter: %s\n", ds1->legend());
      mprintf("\tKeeping rows >= %i and < %i\n", rowmin, rowmax);
      mprintf("\tKeeping cols >= %i and < %i\n", colmin, colmax);
      mprintf("\tCreating new matrix with %i rows and %i columns.\n", nrows, ncols);
      DataSet* ds3 = State.DSL().AddSet( DataSet::MATRIX_DBL, name, "make2d" );
      if (ds3 == 0) return CpptrajState::ERR;
      DataSet_MatrixDbl& matrixOut = static_cast<DataSet_MatrixDbl&>( *ds3 );
      matrixOut.Allocate2D(ncols, nrows);
      matrixOut.SetDim( Dimension::X, Dimension(matrixIn.Dim(0).Coord(colmin),
                                                matrixIn.Dim(0).Step(),
                                                matrixIn.Dim(0).Label()) );
      matrixOut.SetDim( Dimension::Y, Dimension(matrixIn.Dim(1).Coord(rowmin),
                                                matrixIn.Dim(1).Step(),
                                                matrixIn.Dim(1).Label()) );
      for (int row = 0; row < (int)matrixIn.Nrows(); row++)
      {
        if (row >= rowmin && row < rowmax)
        {
          for (int col = 0; col < (int)matrixIn.Ncols(); col++)
          {
            if (col >= colmin && col < colmax)
            {
              double val = matrixIn.GetElement(col, row);
              matrixOut.SetElement( col-colmin, row-rowmin, val );
              //mprintf("DEBUG:\tmatrixIn(%i, %i) = %f  to matrixOut(%i, %i)\n",
              //        col, row, val, col-colmin, row-rowmin);
            }
          }
        }
      }
    }
  // ---------------------------------------------
  } else if (argIn.hasKey("cat")) { // Concatenate two or more data sets
    std::string name = argIn.GetStringKey("name");
    bool use_offset = !argIn.hasKey("nooffset");
    DataSet* ds3 = State.DSL().AddSet( DataSet::XYMESH, name, "CAT" );
    if (ds3 == 0) return CpptrajState::ERR;
    DataSet_1D& out = static_cast<DataSet_1D&>( *ds3 );
    mprintf("\tConcatenating sets into '%s'\n", out.legend());
    if (use_offset)
      mprintf("\tX values will be offset.\n");
    else
      mprintf("\tX values will not be offset.\n");
    std::string dsarg = argIn.GetStringNext();
    double offset = 0.0;
    while (!dsarg.empty()) {
      DataSetList dsl = State.DSL().GetMultipleSets( dsarg );
      double XY[2];
      for (DataSetList::const_iterator ds = dsl.begin(); ds != dsl.end(); ++ds)
      {
        if ( (*ds)->Type() != DataSet::INTEGER &&
             (*ds)->Type() != DataSet::DOUBLE &&
             (*ds)->Type() != DataSet::FLOAT &&
             (*ds)->Type() != DataSet::XYMESH )
        {
          mprintf("Warning: '%s': Concatenation only supported for 1D scalar data sets.\n",
                  (*ds)->legend());
        } else {
          DataSet_1D const& set = static_cast<DataSet_1D const&>( *(*ds) );
          mprintf("\t\t'%s'\n", set.legend());
          for (size_t i = 0; i != set.Size(); i++) {
            XY[0] = set.Xcrd( i ) + offset;
            XY[1] = set.Dval( i );
            out.Add( i, XY ); // NOTE: value of i does not matter for mesh
          }
          if (use_offset) offset = XY[0];
        }
      }
      dsarg = argIn.GetStringNext();
    }
  // ---------------------------------------------
  } else { // Default: change mode/type for one or more sets.
    std::string modeKey = argIn.GetStringKey("mode");
    std::string typeKey = argIn.GetStringKey("type");
    if (modeKey.empty() && typeKey.empty()) {
      mprinterr("Error: No valid keywords specified.\n");
      return CpptrajState::ERR;
    }
    // First determine mode if specified.
    MetaData::scalarMode dmode = MetaData::UNKNOWN_MODE;
    if (!modeKey.empty()) {
      dmode = MetaData::ModeFromKeyword( modeKey );
      if (dmode == MetaData::UNKNOWN_MODE) {
        mprinterr("Error: Invalid mode keyword '%s'\n", modeKey.c_str());
        return CpptrajState::ERR;
      }
    }
    // Next determine type if specified.
    MetaData::scalarType dtype = MetaData::UNDEFINED;
    if (!typeKey.empty()) {
      dtype = MetaData::TypeFromKeyword( typeKey, dmode );
      if (dtype == MetaData::UNDEFINED) {
        mprinterr("Error: Invalid type keyword '%s'\n", typeKey.c_str());
        return CpptrajState::ERR;
      }
    }
    // Additional options for type 'noe'
    AssociatedData_NOE noeData;
    if (dtype == MetaData::NOE) {
      if (noeData.NOE_Args(argIn))
        return CpptrajState::ERR;
    }
    if (dmode != MetaData::UNKNOWN_MODE)
      mprintf("\tDataSet mode = %s\n", MetaData::ModeString(dmode));
    if (dtype != MetaData::UNDEFINED)
      mprintf("\tDataSet type = %s\n", MetaData::TypeString(dtype));
    // Loop over all DataSet arguments 
    std::string ds_arg = argIn.GetStringNext();
    while (!ds_arg.empty()) {
      DataSetList dsl = State.DSL().GetMultipleSets( ds_arg );
      for (DataSetList::const_iterator ds = dsl.begin(); ds != dsl.end(); ++ds)
      {
        if (dmode != MetaData::UNKNOWN_MODE) {
          // Warn if mode does not seem appropriate for the data set type.
          if ( dmode >= MetaData::M_DISTANCE &&
               dmode <= MetaData::M_RMS &&
               (*ds)->Group() != DataSet::SCALAR_1D )
            mprintf("Warning: '%s': Expected scalar 1D data set type for mode '%s'\n",
                    (*ds)->legend(), MetaData::ModeString(dmode));
          else if ( dmode == MetaData::M_VECTOR &&
                    (*ds)->Type() != DataSet::VECTOR )
            mprintf("Warning: '%s': Expected vector data set type for mode '%s'\n",
                    (*ds)->legend(), MetaData::ModeString(dmode));
          else if ( dmode == MetaData::M_MATRIX &&
                    (*ds)->Group() != DataSet::MATRIX_2D )
            mprintf("Warning: '%s': Expected 2D matrix data set type for mode '%s'\n",
                    (*ds)->legend(), MetaData::ModeString(dmode));
        }
        if ( dtype == MetaData::NOE ) (*ds)->AssociateData( &noeData );
        mprintf("\t\t'%s'\n", (*ds)->legend());
        MetaData md = (*ds)->Meta();
        md.SetScalarMode( dmode );
        md.SetScalarType( dtype );
        (*ds)->SetMeta( md );
      }
      ds_arg = argIn.GetStringNext();
    }
  }
  return CpptrajState::OK;
}
