// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:

// headers


/* NOTES

TODO:
remove some or all modifications to residues, and/or remap res numbers? fix hbond distance check issues

// have a hash builder class, could manage threading of inserts
// break into RifGenerator classes for hbond, apo, etc..

*/

	#include <ObjexxFCL/FArray3D.hh>
	#include <ObjexxFCL/format.hh>

	#include <basic/Tracer.hh>
	#include <basic/options/keys/in.OptionKeys.gen.hh>
	#include <basic/options/keys/mh.OptionKeys.gen.hh>
	#include <basic/options/keys/out.OptionKeys.gen.hh>
	#include <basic/options/keys/packing.OptionKeys.gen.hh>
	#include <basic/options/option_macros.hh>

	#include <boost/assign/std/vector.hpp>
	#include <boost/foreach.hpp>
	#include <boost/lexical_cast.hpp>
	#include <boost/math/special_functions/sign.hpp>

	#include <core/id/AtomID.hh>
	#include <core/import_pose/import_pose.hh>
	#include <core/conformation/Residue.hh>
	#include <core/pose/Pose.hh>
	#include <core/pose/PDBInfo.hh>
	#include <core/pose/motif/reference_frames.hh>
	#include <numeric/xyz.io.hh>

	#include <devel/init.hh>
	#include <riflib/RotamerGenerator.hh>
	#include <riflib/rosetta_field.hh>
	#include <riflib/util.hh>
	#include <riflib/util_complex.hh>
	#include <riflib/rotamer_energy_tables.hh>
	#include <riflib/RifFactory.hh>
	// #include <riflib/rif/RifGenerator.hh>
	// #include <riflib/rif/RifAccumulators.hh>
	#include <riflib/rif/RifGeneratorSimpleHbonds.hh>
	#include <riflib/rif/RifGeneratorApoHSearch.hh>
	#include <riflib/rif/RifGeneratorUserHotspots.hh>

	#include <map>
	#include <parallel/algorithm>

	#include <scheme/chemical/RotamerIndex.hh>
	#include <scheme/objective/hash/XformMap.hh>
	#include <scheme/objective/storage/RotamerScores.hh>
	#include <scheme/actor/BackboneActor.hh>

	#include <utility/file/file_sys_util.hh>
	#include <utility/io/izstream.hh>
	#include <utility/io/ozstream.hh>
	#include <utility/tools/make_vector1.hh>

	#include <future>

	// #include <numeric/random/random_permutation.hh>
	// #include <numeric/random/random_xyz.hh>

OPT_1GRP_KEY( StringVector, rifgen, donres )
	OPT_1GRP_KEY( StringVector  , rifgen, accres )
	OPT_1GRP_KEY( Real          , rifgen, tip_tol_deg )
	OPT_1GRP_KEY( Real          , rifgen, rot_samp_resl )
	OPT_1GRP_KEY( Real          , rifgen, rot_samp_range )
	OPT_1GRP_KEY( Boolean       , rifgen, fix_donor )
	OPT_1GRP_KEY( Boolean       , rifgen, fix_acceptor )
	OPT_1GRP_KEY( File          , rifgen, target )
	OPT_1GRP_KEY( File          , rifgen, target_res )
	OPT_1GRP_KEY( Boolean       , rifgen, rif_append_mode )
	OPT_1GRP_KEY( Boolean       , rifgen, append_mode_clear_sats )
	OPT_1GRP_KEY( Real          , rifgen, rif_hbond_dump_fraction )
	OPT_1GRP_KEY( Real          , rifgen, rif_apo_dump_fraction )
	OPT_1GRP_KEY( StringVector  , rifgen, data_cache_dir )
	OPT_1GRP_KEY( Integer       , rifgen, hbgeom_max_cache )
	OPT_1GRP_KEY( Real          , rifgen, rosetta_field_resl )
	OPT_1GRP_KEY( RealVector    , rifgen, search_resolutions )
	OPT_1GRP_KEY( Real          , rifgen, hash_cart_resl )
	OPT_1GRP_KEY( Real          , rifgen, hash_angle_resl )
	OPT_1GRP_KEY( Real          , rifgen, score_threshold )
	OPT_1GRP_KEY( Integer       , rifgen, rf_oversample )
	OPT_1GRP_KEY( Boolean       , rifgen, generate_rf_for_docking )
	OPT_1GRP_KEY( Real          , rifgen, beam_size_M )
	OPT_1GRP_KEY( StringVector  , rifgen, apores )
	OPT_1GRP_KEY( Real          , rifgen, hash_preallocate_mult )
	OPT_1GRP_KEY( Real          , rifgen, score_cut_adjust )
	OPT_1GRP_KEY( String        , rifgen, outfile )
	OPT_1GRP_KEY( String        , rifgen, outdir )
	OPT_1GRP_KEY( StringVector  , rifgen, test_structures )
	OPT_1GRP_KEY( Real          , rifgen, max_rf_bounding_ratio )
	OPT_1GRP_KEY( Real          , rifgen, hbond_cart_sample_hack_range )
	OPT_1GRP_KEY( Real          , rifgen, hbond_cart_sample_hack_resl )
	OPT_1GRP_KEY( Integer       , rifgen, rif_accum_scratch_size_M )
	OPT_1GRP_KEY( Boolean       , rifgen, make_shitty_rpm_file )
	OPT_1GRP_KEY( Boolean       , rifgen, test_without_rosetta_fields )
	OPT_1GRP_KEY( Boolean       , rifgen, downweight_hydrophobics )
	OPT_1GRP_KEY(  Real         , rifgen, hbond_weight )
	OPT_1GRP_KEY(  Real         , rifgen, upweight_multi_hbond )
	OPT_1GRP_KEY( Real          , rifgen, min_hb_quality_for_satisfaction )
	OPT_1GRP_KEY( Real          , rifgen, long_hbond_fudge_distance )
	OPT_1GRP_KEY( Boolean       , rifgen, no_bb_hbonds )
	OPT_1GRP_KEY( IntegerVector , rifgen, repulsive_atoms )
	OPT_1GRP_KEY( String        , rifgen, rif_type )
	OPT_1GRP_KEY( Boolean       , rifgen, extra_rotamers )
	OPT_1GRP_KEY( Boolean       , rifgen, extra_rif_rotamers )
	OPT_1GRP_KEY( Boolean       , rifgen, use_rosetta_grid_energies )
	OPT_1GRP_KEY( Boolean       , rifgen, soft_rosetta_grid_energies )
	OPT_1GRP_KEY( Boolean       , rifgen, dump_bidentate_hbonds )
	OPT_1GRP_KEY( Boolean       , rifgen, report_aa_count )
	OPT_1GRP_KEY( Boolean       , rifgen, dump_rifgen_hdf5 )

	OPT_1GRP_KEY( Boolean       , rifgen, dont_center_hotspots )
	OPT_1GRP_KEY( StringVector  , rifgen, hotspot_groups )
	OPT_1GRP_KEY( String        , rifgen, hotspot_list_file )
    OPT_1GRP_KEY( Real          , rifgen, hotspot_sample_cart_bound )
    OPT_1GRP_KEY( Real          , rifgen, hotspot_sample_angle_bound )
    OPT_1GRP_KEY( Integer       , rifgen, hotspot_nsamples )
    OPT_1GRP_KEY( Real          , rifgen, hotspot_score_thresh )
    OPT_1GRP_KEY( Boolean       , rifgen, hotspot_add_to_rotamer_spec )
    OPT_1GRP_KEY( Real          , rifgen, hotspot_score_override )
    OPT_1GRP_KEY( Integer       , rifgen, dump_hotspot_samples )
    OPT_1GRP_KEY( Boolean       , rifgen, test_hotspot_redundancy )
	OPT_1GRP_KEY( Real          , rifgen, hotspot_score_bonus )
	OPT_1GRP_KEY( Boolean       , rifgen, label_hotspots_254 )
	OPT_1GRP_KEY( Boolean       , rifgen, all_hotspots_are_bidentate )
    OPT_1GRP_KEY( Boolean		, rifgen, single_file_hotspots_insertion)
    OPT_1GRP_KEY( Boolean		, rifgen, clear_sats_before_hotspots)
    OPT_1GRP_KEY( Boolean		, rifgen, use_d_aa)
    OPT_1GRP_KEY( Boolean		, rifgen, use_l_aa)

	// bounding grids stuff
	OPT_1GRP_KEY( RealVector        , rifgen, hash_cart_resls        )
	OPT_1GRP_KEY( RealVector        , rifgen, hash_cart_bounds       )
	OPT_1GRP_KEY( RealVector        , rifgen, hash_ang_resls         )
	OPT_1GRP_KEY( RealVector        , rifgen, lever_radii      )
	OPT_1GRP_KEY( RealVector        , rifgen, lever_bounds     )


  // the tuning file, finely control how the rifgen and rifdock works
  OPT_1GRP_KEY( String        , rifgen, tuning_file )
  OPT_1GRP_KEY( Boolean       , rifgen, only_place_requirement_res )

  OPT_1GRP_KEY( Real          , rifgen, min_cationpi_score     )
  OPT_1GRP_KEY( Real          , rifgen, cationpi_bonus_weights )



	void register_options() {
		using namespace basic::options;
		using namespace basic::options::OptionKeys;
		NEW_OPT(  rifgen::donres                           , "" , utility::vector1<std::string>() );
		NEW_OPT(  rifgen::accres                           , "" , utility::vector1<std::string>() );
		NEW_OPT(  rifgen::tip_tol_deg                      , "" , 30.0 );
		NEW_OPT(  rifgen::rot_samp_range                   , "" , 360.0 );
		NEW_OPT(  rifgen::rot_samp_resl                    , "" ,  7.0 );
		NEW_OPT(  rifgen::fix_donor                        , "" ,  false );
		NEW_OPT(  rifgen::fix_acceptor                     , "" ,  false );
		NEW_OPT(  rifgen::target                           , "" , "" );
		NEW_OPT(  rifgen::target_res                       , "" , "" );
		NEW_OPT(  rifgen::rif_append_mode                  , "Add to an already existing rif. Modifies in place.", false );
		NEW_OPT(  rifgen::append_mode_clear_sats           , "Clear all previous sats when adding to rif", false );
		NEW_OPT(  rifgen::rif_hbond_dump_fraction          , "" , 0.0001 );
		NEW_OPT(  rifgen::rif_apo_dump_fraction            , "" , 0.0001 );
		NEW_OPT(  rifgen::data_cache_dir                   , "" , utility::vector1<std::string>(1,"./") );
		NEW_OPT(  rifgen::hbgeom_max_cache                 , "max number of geom files to load at once", -1 );
		NEW_OPT(  rifgen::rosetta_field_resl               , "" , 0.5 );
		NEW_OPT(  rifgen::search_resolutions               , "" , utility::vector1<core::Real>() );
		NEW_OPT(  rifgen::hash_cart_resl                   , "" , 0.2 );
		NEW_OPT(  rifgen::hash_angle_resl                  , "" , 1.5 );
		NEW_OPT(  rifgen::score_threshold                  , "" , -0.5 );
		NEW_OPT(  rifgen::rf_oversample                    , "" , 2 );
		NEW_OPT(  rifgen::generate_rf_for_docking          , "" , true );
		NEW_OPT(  rifgen::beam_size_M                      , "" , 10.000000 );
		NEW_OPT(  rifgen::score_cut_adjust                   , "" , 1.0 );
		NEW_OPT(  rifgen::apores                           , "" , utility::vector1<std::string>() );
		NEW_OPT(  rifgen::hash_preallocate_mult            , "" , 1.0 );
		NEW_OPT(  rifgen::outfile                          , "" , "default_rif_hier_outfile.rif.gz" );
		NEW_OPT(  rifgen::outdir                           , "" , "./default_rif_hier_outdir" );
		NEW_OPT(  rifgen::test_structures                  , "" , utility::vector1<std::string>() );
		NEW_OPT(  rifgen::max_rf_bounding_ratio            , "" , 4 );
		NEW_OPT(  rifgen::hbond_cart_sample_hack_range     , "" , 0.375 );
		NEW_OPT(  rifgen::hbond_cart_sample_hack_resl      , "" , 0.375 );
		NEW_OPT(  rifgen::rif_accum_scratch_size_M         , "" , 32000 );
		NEW_OPT(  rifgen::make_shitty_rpm_file             , "" , false );
		NEW_OPT(  rifgen::test_without_rosetta_fields      , "" , false );
		NEW_OPT(  rifgen::downweight_hydrophobics          , "" , false );
		NEW_OPT(  rifgen::hbond_weight                     , "" , 2.0 );
		NEW_OPT(  rifgen::upweight_multi_hbond             , "" , 0.0 );
		NEW_OPT(  rifgen::min_hb_quality_for_satisfaction  , "" , -0.6 );
		NEW_OPT(  rifgen::long_hbond_fudge_distance        , "Any hbond longer than 2A gets moved closer to 2A by this amount for scoring", 0.0 );
		NEW_OPT(  rifgen::no_bb_hbonds                     , "Disable generation of hbonds to target backbone atoms." , false );
		NEW_OPT(  rifgen::repulsive_atoms                  , "" , utility::vector1<int>() );
		NEW_OPT(  rifgen::rif_type                         , "" , "RotScore" );
		NEW_OPT(  rifgen::extra_rotamers                   , "" , true );
		NEW_OPT(  rifgen::extra_rif_rotamers               , "" , true );
		NEW_OPT(  rifgen::use_rosetta_grid_energies        , "Use Frank's grid energies for scoring polar residues", false );
		NEW_OPT(  rifgen::soft_rosetta_grid_energies       , "Use soft option for grid energies", false );
		NEW_OPT(  rifgen::dump_bidentate_hbonds            , "Dump all bidentate hbonds", false );
		NEW_OPT(  rifgen::report_aa_count                  , "Really hacky thing to report aa count during rifgen", false );
		NEW_OPT(  rifgen::dump_rifgen_hdf5                 , "Dump the rif to an hdf5 file.", false );
		
		NEW_OPT(  rifgen::dont_center_hotspots             , "Nate added this flag and it may not work" , false );
		NEW_OPT(  rifgen::hotspot_groups                   , "" , utility::vector1<std::string>() );
		NEW_OPT(  rifgen::hotspot_list_file                , "" , "" );
		NEW_OPT(  rifgen::hotspot_sample_cart_bound        , "" , 0.5 );
        NEW_OPT(  rifgen::hotspot_sample_angle_bound       , "" , 15.0 );
        NEW_OPT(  rifgen::hotspot_nsamples                 , "" , 10000 );
        NEW_OPT(  rifgen::hotspot_score_thresh             , "" , 5.0 );
        NEW_OPT(  rifgen::hotspot_add_to_rotamer_spec      , "Add hotspot input rotamers to global list of rotamers." , true );
        NEW_OPT(  rifgen::hotspot_score_override           , "Override score for hotspots. 12345 to disable." , 12345 );
        NEW_OPT(  rifgen::dump_hotspot_samples             , "" , 1000 );
        NEW_OPT(  rifgen::test_hotspot_redundancy          , "Determine if hotspots are already in rif and if they are self-redundant. This makes an invalid RIF!!!", false );
		NEW_OPT(  rifgen::hotspot_score_bonus              , "Amount to add to rif score (you probably want to use a negative number) ", 0. );
		NEW_OPT(  rifgen::label_hotspots_254               , "True if hotspots to be labeled with sat 254 in log ", false );
        NEW_OPT(  rifgen::all_hotspots_are_bidentate       , "Hotpots only inserted if they make bidentate h-bonds", false );
        NEW_OPT(  rifgen::single_file_hotspots_insertion	, "" , false);
		NEW_OPT(  rifgen::clear_sats_before_hotspots        , "Clear all previous sats before adding hotspots", false );
        NEW_OPT(  rifgen::use_d_aa							, "" , false);
        NEW_OPT(  rifgen::use_l_aa							, "" , true);



		// make bounding grids stuff
		NEW_OPT( rifgen::hash_cart_resls, "cartesian resolution(s) of hash table(s)"      , utility::vector1<double>() );
		NEW_OPT( rifgen::hash_cart_bounds, "bound on cartesian coordinates"               , utility::vector1<double>() );
		NEW_OPT( rifgen::hash_ang_resls,  "ang reslolution(s) of hash table(s) in degrees", utility::vector1<double>() );
		NEW_OPT( rifgen::lever_radii      , ""                                      , utility::vector1<double>() );
		NEW_OPT( rifgen::lever_bounds     , ""                                      , utility::vector1<double>() );


		NEW_OPT(  rifgen::tuning_file                          , "precisely control how rifgen and rifdock work" , "" );
		NEW_OPT(  rifgen::only_place_requirement_res           , "if it doesn't satisfy the tuning file, don't place it in the apo search", false );

		NEW_OPT(  rifgen::min_cationpi_score    	, "score used to filter bad cationpi rif residues" , -0.2);
    NEW_OPT(  rifgen::cationpi_bonus_weights	, "final cationpi score is apo_score+weights*cationpi_score capped to -9.0" , 6.0);
	}



void makedircheck( std::string dir ){
	if( ! utility::file::file_exists( dir) ){
		std::cout << "does not exist, attempting to create:" << std::endl;
		std::cout << "    " << dir << std::endl;
		utility::file::create_directory_recursive(dir);
		if( ! utility::file::file_exists( dir) ){
			utility_exit_with_message("missing data dir: '" + dir + "'" );
		}
	}
}








std::string
make_bounding_grids(
	std::shared_ptr<::devel::scheme::RifFactory> rif_factory,
	::devel::scheme::RifPtr ref_rif,
	std::string ref_description,
	std::string fname_base,
	int ibound
){
	std::string fname;

	using namespace basic::options;
	using ObjexxFCL::format::F;
	namespace ons = basic::options::OptionKeys::rifgen;
	using namespace devel::scheme;
	using std::cout;
	using std::endl;

	// for( int ibound = 1; ibound <= option[ons::lever_bounds]().size(); ++ibound ){

		double const lever_radius      = option[ons::lever_radii      ]().at( ibound );
		double const lever_bound       = option[ons::lever_bounds     ]().at( ibound );
		double const hash_ang_resl     = option[ons::hash_ang_resls   ]().at( ibound );
		double const hash_cart_resl    = option[ons::hash_cart_resls  ]().at( ibound );
		double const hash_cart_bound   = option[ons::hash_cart_bounds ]().at( ibound );

		double const cart_bound = lever_bound;
		double const  ang_bound_rad = lever_bound / lever_radius;
		double const  ang_bound = ang_bound_rad * 180.0 / M_PI;

		RifPtr new_rif = rif_factory->create_rif_from_rif( ref_rif, hash_cart_resl, hash_ang_resl, hash_cart_bound );

		#pragma omp critical
		{
			cout << "make_bounding_gird: "
			     << "cart " << cart_bound << " / " << hash_cart_resl
			     << ", ang: " << ang_bound << " / "  << hash_ang_resl
			     <<  endl;
			cout << "    new map size " << KMGT(new_rif->size())
				 << " ratio: " <<  (float)new_rif->size() / (float)ref_rif->size() << std::endl;
			cout << "    mem: " << KMGT( new_rif->mem_use() )
			     << " load: " << new_rif->load_factor()
				 << ", sizeof(value_type) " << new_rif->sizeof_value_type() << endl;
		}

		std::string digits = boost::lexical_cast<std::string>(lever_bound);
		if( digits.size() == 1 ) digits = "0" + digits;
		std::string tag = "_BOUNDING_";
		std::ostringstream oss_description;
		oss_description << "==== bounding xmap ====" << endl;
		oss_description << "!!!!!!!!!!!!!!!!!!!!!!!!!!!! USING_HACKY_GRIDS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
		oss_description << "hash_cart_bound: " << hash_cart_bound << endl;
		oss_description << "lever_radius:  " << lever_radius << endl;
		oss_description << "lever_bound:   " << lever_bound << endl;
		oss_description << "==== source oss_description ====\n" << ref_description;
		std::string description = oss_description.str();

		fname = fname_base+tag+"RIF_"+digits + ".xmap.gz";
		utility::io::ozstream out( fname );
		new_rif->save( out, description );
		out.close();

	return fname;
}

std::shared_ptr<::devel::scheme::RifBase> init_rif_and_generators(
		std::shared_ptr<::devel::scheme::RifFactory> rif_factory,
		std::vector< std::vector< devel::scheme::VoxelArrayPtr > > & bounding_by_atype,
		std::vector<float> & RESLS,
		numeric::xyzVector<core::Real> target_center,
		std::string outfile,
		std::vector< ::scheme::shared_ptr<devel::scheme::rif::RifGenerator> > & rif_generators_out,
		bool & needs_donors_acceptors
	){
	using basic::options::option;
		using namespace basic::options::OptionKeys;
			using core::id::AtomID;
			using std::cout;
			using std::endl;
			using namespace devel::scheme;
			// typedef numeric::xyzVector<core::Real> Vec;
			// typedef numeric::xyzVector<float> Vecf;
			// typedef numeric::xyzMatrix<core::Real> Mat;
			// typedef numeric::xyzTransform<core::Real> Xform;
			// using ObjexxFCL::format::F;
			// using ObjexxFCL::format::I;
			// using devel::scheme::KMGT;


	std::shared_ptr<RifBase> rif = rif_factory->create_rif();

	bool rif_exists = false;
	if( utility::file::file_exists( outfile) ){
		std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
		std::cout << "!!!!! RIF file already exists: " << outfile << " !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
		std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
		utility::io::izstream infile( outfile , std::ios::binary );
		std::string tmp;
		runtime_assert( rif->load( infile, tmp ) );
		std::cout << "   done loading: " << outfile << std::endl;
		cout << "RIF size: " << KMGT( rif->mem_use() ) << " load: " << rif->load_factor()
			  << ", sizeof(value_type) " << rif->sizeof_value_type() << endl;

		rif_exists = true;
	}

	if ( option[ rifgen::rif_append_mode ]() ) {
		if ( ! rif_exists ) {
			utility_exit_with_message("-rifgen::rif_mode_append used but no rif loaded!!!");
		}

		if ( option[ rifgen::append_mode_clear_sats ]() ) {
			std::cout << "Clearing sats..." << std::endl;
			runtime_assert( rif->has_sat_data_slots() );
			rif->clear_sats();
		}
	}

	if ( ! rif_exists || option[ rifgen::rif_append_mode ]() ) {

		//////////////////////////////////// RIF HBOND gen setup //////////////////////////////////////////
		bool do_hbond  = option[rifgen::donres]().size() > 0;
			 do_hbond |= option[rifgen::accres]().size() > 0;
			 do_hbond &= option[rifgen::hbond_weight]() > 0;
		if( do_hbond ){
			needs_donors_acceptors = rif->has_sat_data_slots();

			devel::scheme::rif::RifGeneratorSimpleHbondsOpts hbgenopts;
			hbgenopts.tip_tol_deg = option[ rifgen::tip_tol_deg ]();
			hbgenopts.rot_samp_resl = option[ rifgen::rot_samp_resl ]();
			hbgenopts.rot_samp_range = option[ rifgen::rot_samp_range ]();
			hbgenopts.hbond_cart_sample_hack_range = option[ rifgen::hbond_cart_sample_hack_range ]();
			hbgenopts.hbond_cart_sample_hack_resl = option[ rifgen::hbond_cart_sample_hack_resl ]();
			hbgenopts.score_threshold = option[ rifgen::score_threshold ]();
			hbgenopts.dump_fraction = option[rifgen::rif_hbond_dump_fraction]();
			hbgenopts.debug = false;
			hbgenopts.dump_bindentate_hbonds = option[ rifgen::dump_bidentate_hbonds ]();
			hbgenopts.report_aa_count = option[ rifgen::report_aa_count ]();
			hbgenopts.hbgeom_max_cache = option[ rifgen::hbgeom_max_cache ]();
			hbgenopts.no_bb_hbonds = option[ rifgen::no_bb_hbonds ]();

			rif_generators_out.push_back(
				::scheme::make_shared<devel::scheme::rif::RifGeneratorSimpleHbonds>(
					  option[ rifgen::donres ]()
					, option[ rifgen::accres ]()
					, hbgenopts
				)
			);

		}

		////////////////////////// apo gen setup //////////////////////////////
		if( option[rifgen::apores]().size() ) {

			devel::scheme::rif::RifGeneratorApoHSearchOpts apogenopts;
			apogenopts.dump_fraction = option[rifgen::rif_apo_dump_fraction]();
			apogenopts.score_cut_adjust = option[rifgen::score_cut_adjust]();
			apogenopts.abs_score_cut = option[rifgen::score_threshold]();
			apogenopts.downweight_hydrophobics = option[rifgen::downweight_hydrophobics]();
			apogenopts.beam_size_M = option[rifgen::beam_size_M]();
			apogenopts.dump_fraction = option[rifgen::rif_apo_dump_fraction]();
			apogenopts.only_place_requirement_res = option[rifgen::only_place_requirement_res]();
			apogenopts.min_cationpi_score  = option[rifgen::min_cationpi_score]();
			apogenopts.cationpi_bonus_weights  = option[rifgen::cationpi_bonus_weights]();

			rif_generators_out.push_back(
				::scheme::make_shared<devel::scheme::rif::RifGeneratorApoHSearch>(
					  option[ rifgen::apores ]()
					, bounding_by_atype
					, RESLS
					, apogenopts
				)
			);

		}

		if( option[rifgen::hotspot_groups]().size() || option[rifgen::hotspot_list_file].user() ){
			devel::scheme::rif::RifGeneratorUserHotspotsOpts hspot_opts;
			auto const & hspot_files = option[rifgen::hotspot_groups]();
            std::string hspot_list_file = option[rifgen::hotspot_list_file]();
            
            if (hspot_list_file != "") {
                // read file contents into hspot_files
                runtime_assert_msg(utility::file::file_exists( hspot_list_file ), "hotspot list file does not exist: " + hspot_list_file );
                auto hspot_files = utility::vector1<std::string>();
                
                std::ifstream in;
                in.open(hspot_list_file, std::ios::in);
                std::string hspot_fname;
                while (std::getline(in, hspot_fname)) {
                    if (hspot_fname.empty()) continue;
                    
                    hspot_fname = utility::strip(hspot_fname, "\n");
                    runtime_assert_msg(utility::file::file_exists( hspot_fname ), "hotspot file does not exist: " + hspot_fname );
                    hspot_files.push_back(hspot_fname);
                }
                hspot_opts.hotspot_files.insert( hspot_opts.hotspot_files.end(), hspot_files.begin(), hspot_files.end() );
            } else {
                hspot_opts.hotspot_files.insert( hspot_opts.hotspot_files.end(), hspot_files.begin(), hspot_files.end() );
            }
        	
        	hspot_opts.dont_center_hotspots = option[ rifgen::dont_center_hotspots]();
			hspot_opts.hotspot_sample_cart_bound = option[ rifgen::hotspot_sample_cart_bound ]();
            hspot_opts.hotspot_sample_angle_bound = option[ rifgen::hotspot_sample_angle_bound]();
            hspot_opts.hotspot_nsamples = option[ rifgen::hotspot_nsamples]();
            hspot_opts.hotspot_score_thresh = option[ rifgen::hotspot_score_thresh]();
            hspot_opts.dump_hotspot_samples = option[ rifgen::dump_hotspot_samples]();
            hspot_opts.test_hotspot_redundancy = option[ rifgen::test_hotspot_redundancy]();
			hspot_opts.hotspot_score_bonus = option[ rifgen::hotspot_score_bonus ]();
			hspot_opts.label_hotspots_254 = option[ rifgen::label_hotspots_254 ]();
            hspot_opts.all_hotspots_are_bidentate = option[ rifgen::all_hotspots_are_bidentate]();
            hspot_opts.use_d_aa = option[rifgen::use_d_aa]();
            hspot_opts.add_to_rotamer_spec = option[rifgen::hotspot_add_to_rotamer_spec]();
            hspot_opts.hotspot_score_override = option[rifgen::hotspot_score_override]();
            hspot_opts.clear_sats_first = option[rifgen::clear_sats_before_hotspots]();
			if (!option[ rifgen::dump_hotspot_samples].user()) hspot_opts.dump_hotspot_samples = 0;
			hspot_opts.single_file_hotspots_insertion = option[rifgen::single_file_hotspots_insertion]();
			for(int i = 0; i < 3; ++i) hspot_opts.target_center[i] = target_center[i];
			rif_generators_out.push_back( make_shared<devel::scheme::rif::RifGeneratorUserHotspots>( hspot_opts ) );
		}
	}

	return rif;
}



int main(int argc, char *argv[]) {

	omp_lock_t cout_lock, io_lock, pose_lock, hbond_geoms_cache_lock;
	omp_init_lock( &cout_lock );
	omp_init_lock( &io_lock );
	omp_init_lock( &pose_lock );
	omp_init_lock( &hbond_geoms_cache_lock );

	register_options();
	devel::init(argc,argv);

	devel::scheme::fix_omp_max_threads();
	std::cout << "Rifdock thinks there are " << devel::scheme::omp_max_threads() << " threads." << std::endl;

	using basic::options::option;
		using namespace basic::options::OptionKeys;
		using namespace core::scoring;
			using core::id::AtomID;
			using std::cout;
			using std::endl;
			using namespace devel::scheme;
			typedef numeric::xyzVector<core::Real> Vec;
			typedef numeric::xyzVector<float> Vecf;
			typedef numeric::xyzMatrix<core::Real> Mat;
			typedef numeric::xyzTransform<core::Real> Xform;
			using ObjexxFCL::format::F;
			using ObjexxFCL::format::I;
			using devel::scheme::KMGT;


	std::string rif_type = option[rifgen::rif_type]();
	std::string target_reslist_file = basic::options::option[basic::options::OptionKeys::rifgen::target_res]();
	std::cout << "rif_type: " << rif_type << std::endl;


	devel::scheme::RifFactoryConfig rif_factory_config;
	rif_factory_config.rif_type = rif_type;
	shared_ptr<RifFactory> rif_factory = ::devel::scheme::create_rif_factory( rif_factory_config );


	runtime_assert_msg( option[rifgen::rif_hbond_dump_fraction]() < 0.011 , "-rif_hbond_dump_fraction should be small, or you will be very sad..." );
	runtime_assert_msg( option[rifgen::rif_apo_dump_fraction]()   < 0.011 , "-rif_apo_dump_fraction should be small, or you will be very sad..." );

	runtime_assert_msg( option[rifgen::min_hb_quality_for_satisfaction]() < 0 && option[rifgen::min_hb_quality_for_satisfaction]() > -1, 
		"-min_hb_quality_for_satisfaction must be between -1 and 0");

	std::string outdir = option[ rifgen::outdir ]();
	std::string outfile = outdir + "/" + option[ rifgen::outfile ]();
	runtime_assert_msg( outfile != "", "rifgen::outfine is blank!");
	runtime_assert_msg( outfile.find('/') != outfile.size(), "rifgen:outfile should not contain '/'!");

	std::vector<std::string> bounding_grid_fnames;

	std::vector<std::string> cache_data_path;
	BOOST_FOREACH( std::string dir, option[rifgen::data_cache_dir]() ){
		cache_data_path.push_back( dir );
	}

	std::string target_fname = option[rifgen::target]();
	float rf_resl = option[rifgen::rosetta_field_resl]();
	if( ! option[rifgen::target].user() ) utility_exit_with_message("must specify -rifgen:target!");

	bool const replace_all_with_ala_1bre = true; // replace all res on scaffold / test_structure with ALA for 1be calc

	makedircheck( outdir );
	// std::string outfile = option[ rifgen::outfile ]();
	// int odidx = outfile.rfind("/");
	// std::string outdir = outfile.substr(0,odidx);
	// makedircheck( outdir );

	std::vector<float> RESLS;
	BOOST_FOREACH( float r, option[rifgen::search_resolutions]() ) RESLS.push_back(r);
	// runtime_assert( fabs( rf_resl - RESLS.back() ) < 0.0001 );

	print_header( "preparing target" );
	core::pose::PoseOP target = make_shared<core::pose::Pose>();
	core::import_pose::pose_from_file( *target, target_fname );
	std::cout << "target nres: " << target->size() << std::endl;
	std::string target_tag = utility::file::file_basename( utility::file_basename( target_fname ) );

	Vec target_center(0,0,0);
	utility::vector1<core::Size> target_res = get_res( target_reslist_file , *target, /*nocgp*/false );
	{
		std::cout << "target_res_file: '" << target_reslist_file << "'" << std::endl;
		std::cout << "target_res: " << target_res << std::endl;
		core::pose::Pose target0 = *target;
		int count = 0;
		BOOST_FOREACH( core::Size ir, target_res ){
			for( int ia = 1; ia <= target0.residue(ir).nheavyatoms(); ++ia ){
				// std::cout << ir << " " << ia << " " << target0.residue(ir).xyz(ia) << std::endl;
				target_center += target0.residue(ir).xyz(ia);
				++count;
			}
		}
		target_center /= (double)count;
		cout << "centering target from " << target_center << " to ( 0, 0, 0 )" << endl;
		*target = target0;
		for( int ir = 1; ir <= target0.size(); ++ir ){
			for( int ia = 1; ia <= target0.residue_type(ir).natoms(); ++ia ){
				target->set_xyz( core::id::AtomID(ia,ir), target0.residue(ir).xyz(ia) - target_center );
			}
		}
	}
	std::string centered_target_pdbfile = outfile + "_target.pdb.gz";
	target->dump_pdb( centered_target_pdbfile );

			/// shitty test code to look for particular backbones
				float test_rms2_cut = 0.7*0.7;
				std::vector< std::vector< Eigen::Vector3f > > test_bbs;
				if( option[rifgen::test_structures]().size() > 0 ){
					using namespace boost::assign;
					core::pose::Pose test;
					core::import_pose::pose_from_file( test, option[rifgen::test_structures]()[1] );
					for( int ir = 1; ir <= test.size(); ++ir ){
						for( int ia = 1; ia <= test.residue_type(ir).natoms(); ++ia ){
							core::id::AtomID aid(ia,ir);
							test.set_xyz( aid, test.xyz( aid ) - target_center );
						}
					}
					std::vector<int> values; //values += 79,95;//10,30; // add bb positions to look for here
					BOOST_FOREACH( int i, values ){
						std::vector<Eigen::Vector3f> tmp;
						tmp.push_back( Eigen::Vector3f( test.residue(i).xyz("N" ).x(), test.residue(i).xyz("N" ).y(), test.residue(i).xyz("N" ).z() ) );
						tmp.push_back( Eigen::Vector3f( test.residue(i).xyz("CA").x(), test.residue(i).xyz("CA").y(), test.residue(i).xyz("CA").z() ) );
						tmp.push_back( Eigen::Vector3f( test.residue(i).xyz("C" ).x(), test.residue(i).xyz("C" ).y(), test.residue(i).xyz("C" ).z() ) );
						// cout << "add test BB N  " << i << " " << tmp[0].transpose() << endl;
						// cout << "add test BB CA " << i << " " << tmp[1].transpose() << endl;
						// cout << "add test BB C  " << i << " " << tmp[2].transpose() << endl << endl;
						test_bbs.push_back( tmp );
					}
					// You clicked /btn_strep//A/LEU`25/CA
					// You clicked /btn_strep//A/TRP`79/CD2
					// You clicked /btn_strep//A/THR`90/CG2
					// You clicked /btn_strep//A/TRP`92/CB
					// You clicked /btn_strep//A/TRP`108/CE2
				}

#ifdef USEGRIDSCORE
	shared_ptr<protocols::ligand_docking::ga_ligand_dock::GridScorer> grid_scorer;
	if ( option[rifgen::use_rosetta_grid_energies]() ) {
		print_header( "preparing rosetta energy grids" );
		grid_scorer = prepare_grid_scorer( *target, target_res );
	}
#else
	if ( option[rifgen::use_rosetta_grid_energies]() ) {
		utility_exit_with_message( "You must build with -DUSEGRIDSCORE=1 to use -use_rosetta_grid_energies!!!");
	}
#endif

	std::vector< ::scheme::shared_ptr<devel::scheme::rif::RifGenerator> > generators;
	//temp move here for testing 
	std::vector< std::vector< VoxelArrayPtr > > bounding_by_atype( RESLS.size() );
	// std::shared_ptr<::devel::scheme::RifFactory> rif_factory,
	// 	std::vector< std::vector< devel::scheme::VoxelArrayPtr > > & bounding_by_atype,
	// 	std::vector<float> & RESLS,
	// 	numeric::xyzVector<core::Real> target_center,
	// 	std::string outfile,
	// 	std::vector< ::scheme::shared_ptr<devel::scheme::rif::RifGenerator> > & rif_generators_out
	bool needs_donors_acceptors = false;
	
	shared_ptr<RifBase> rif = init_rif_and_generators(rif_factory, bounding_by_atype, RESLS, target_center, outfile, generators, needs_donors_acceptors );

	
	::scheme::chemical::RotamerIndexSpec rot_index_spec;
	std::string rot_spec_fname = outdir +"/rotamer_index_spec.txt";

	if ( option[rifgen::rif_append_mode]() ) {
		std::cout << "Loading " << rot_spec_fname << "..." << std::endl;
		utility::io::izstream infile(rot_spec_fname);
		if ( ! infile ) {
			utility_exit_with_message("Could not load rotamer_index_spec!");
		}
		rot_index_spec.load(infile);
	} else {
		std::cout << "Preparing rotamer index spec..." << std::endl;
		get_rotamer_spec_default(rot_index_spec,option[rifgen::extra_rotamers](), option[rifgen::extra_rif_rotamers](), option[rifgen::use_d_aa](), option[rifgen::use_l_aa]());
	}
	// 	std::string rot_spec_fname = outdir +"/rotamer_index_spec.txt";
	// 	utility::io::ozstream spec_out(rot_spec_fname);
	// 	rot_index_spec.save(spec_out);
	// 	spec_out.close();
	// utility_exit_with_message("exit check");
	for( int igen = 0; igen < generators.size(); ++igen )
	{
		//cache the input 
		generators[igen]->modify_rotamer_spec( rot_index_spec );
	}
	utility::io::ozstream spec_out(rot_spec_fname);
	rot_index_spec.save(spec_out);
	spec_out.close();

	shared_ptr<RotamerIndex> rot_index_p = ::devel::scheme::get_rotamer_index( rot_index_spec, option[rifgen::use_rosetta_grid_energies]() );
		RotamerIndex & rot_index( *rot_index_p );

	// std::cerr << rot_index.size() << std::endl;

	// for( int irot = 0; irot < rot_index.n_primary_rotamers(); ++irot ){
	// 	if (rot_index.resname(irot) == "DTY" || rot_index.resname(irot) == "HIS" || rot_index.resname(irot) == "LYS") {
	// 		utility::io::ozstream out( "rotamer_" + rot_index.resname(irot) + str(irot) + ".pdb" );
	// 		rot_index.dump_pdb_with_children(out,irot);
	// 		out.close();
	// 	}
	// }

	// for( int irot = 0; irot < rot_index.size(); ++irot ){
	//  	if (rot_index.resname(irot) == "DTY" || rot_index.resname(irot) == "HIS" || rot_index.resname(irot) == "LYS") {
	// 	 	utility::io::ozstream out( "rotalnstruct_" + rot_index.resname(irot) + str(irot) + ".pdb" );
	// 	 	rot_index.dump_pdb(out, irot, rot_index.to_structural_parent_frame_.at(irot) );
	// 	 	out.close();
	//  	}
	// }

	// for( int isp : rot_index.structural_parents_ ){
	// 	if (rot_index.resname(isp) == "DTY" || rot_index.resname(isp) == "HIS" || rot_index.resname(isp) == "LYS") {
	// 		utility::io::ozstream out( "rotstructgroup_" + rot_index.resname(isp) + str(isp) + ".pdb" );
	// 		rot_index.dump_pdb_by_structure(out,isp);
	// 		out.close();
	// 	}
	// }

	//utility_exit_with_message("testing out extra rotamers");

		
		// utility::io::ozstream riout( "trp_rots.pdb" );
		// rot_index.dump_pdb( riout, "TRP" );
		// riout.close();
		// utility_exit_with_message("aroitn");
		cout << "======================================================================================" << endl;
		cout << rot_index << endl;
		cout << "======================================================================================" << endl;


	// shared_ptr< rif::RifAccumulator > rif_accum = make_shared< rif::RIFAccumulatorMapThreaded<XMap> >(
	// 	rif_factory,
	// 	option[rifgen::hash_cart_resl](),
	// 	option[rifgen::hash_angle_resl](),
	// 	512.0f,
	// 	option[rifgen::rif_accum_scratch_size_M]()
	// );
	shared_ptr< rif::RifAccumulator > rif_accum = rif_factory->create_rif_accumulator(
		option[rifgen::hash_cart_resl](),
		option[rifgen::hash_angle_resl](),
		512.0f,
		option[rifgen::rif_accum_scratch_size_M]()
	);

	if ( option[rifgen::rif_append_mode]() ) {
		runtime_assert( rif_accum->initialize_with_rif( rif ) );
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// make or read bounding grids
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// typedef boost::shared_ptr< VoxelArray > VoxelArrayPtr;
	std::string fname_grids_for_docking;
	// typedef VoxelArray* VoxelArrayPtr;
	std::vector< VoxelArrayPtr > field_by_atype;
	

	if( option[rifgen::test_without_rosetta_fields]() ){
		std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
		std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! must re-enable reading rosetta_fields !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
		// fill with dummy data for testing...
		for( int itype = 0; itype <= 25; ++itype ){
			field_by_atype.push_back( new VoxelArray( -10000, 10000, 1000 ) );
			for( int iresl = 0; iresl < RESLS.size(); ++iresl ){
				bounding_by_atype[iresl].push_back( new VoxelArray( -10000, 10000, 1000 ) );
			}
		}
		std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
	} else {
		// VDW + SOL score grid
		std::vector<core::id::AtomID> repl_only_atoms;
		parse_atomids( *target, option[rifgen::repulsive_atoms](), repl_only_atoms, "FORCE_REPL_ONLY" );
		{
			devel::scheme::RosettaFieldOptions rfopts;
			rfopts.field_resl = rf_resl;
			rfopts.data_dir = outdir;
			rfopts.oversample = option[rifgen::rf_oversample]();
			rfopts.block_hbond_sites = false;
			rfopts.max_bounding_ratio = option[rifgen::max_rf_bounding_ratio]();
			rfopts.repulsive_only_boundary = true;
			rfopts.repulsive_atoms = repl_only_atoms;
			std::string cache_prefix = devel::scheme::get_rosetta_bounding_fields(
				RESLS,
				target_fname+"_CEN",
				*target,
				target_res,
				rfopts,
				field_by_atype,
				bounding_by_atype,
				false
			);
			fname_grids_for_docking = cache_prefix;
		}
		std::cout << "done with rifgen grids, not make target ones for docking" << std::endl;
		if( option[rifgen::generate_rf_for_docking]() ){
			std::vector<float>  DOCK_RESLS;
			DOCK_RESLS.push_back( 16.0 );
			DOCK_RESLS.push_back(  8.0 );
			DOCK_RESLS.push_back(  4.0 );
			DOCK_RESLS.push_back(  2.0 );
			DOCK_RESLS.push_back(  1.0 );
			DOCK_RESLS.push_back(  0.5 );
			devel::scheme::RosettaFieldOptions rfopts2;
			rfopts2.field_resl = rf_resl;
			rfopts2.data_dir = outdir;
			rfopts2.oversample = option[rifgen::rf_oversample]();
			rfopts2.block_hbond_sites = false;
			rfopts2.max_bounding_ratio = option[rifgen::max_rf_bounding_ratio]();
			rfopts2.repulsive_only_boundary = true; // default
			rfopts2.repulsive_atoms = repl_only_atoms;
			rfopts2.generate_only = true;
			std::vector< std::vector< VoxelArrayPtr > > DUMMY( RESLS.size() );
			devel::scheme::get_rosetta_bounding_fields_from_fba(
				DOCK_RESLS,
				target_fname+"_CEN",
				*target,
				target_res,
				rfopts2,
				field_by_atype,
				DUMMY,
				false,
				fname_grids_for_docking
			);
			for( auto & vv : DUMMY ) for( auto vp : vv ) if(vp) delete vp;

		}
	}


	//////////////////////////////// make target scorer ///////////////////////////////////////////////
	HBRayOpts hbopt;

    std::vector<std::pair<int, std::string> > target_donor_names;
    std::vector<std::pair<int, std::string> > target_acceptor_names;
	std::vector< ::scheme::chemical::HBondRay > target_donors, target_acceptors;
	for( auto ir : target_res ){
        ::devel::scheme::get_donor_rays   ( *target, ir, hbopt, target_donors, target_donor_names );
        ::devel::scheme::get_acceptor_rays( *target, ir, hbopt, target_acceptors, target_acceptor_names );
	}

	// these get used now. Don't change the names!!!
	utility::io::ozstream donout(outfile+"_"+"donors.pdb.gz");
	::devel::scheme::dump_hbond_rays( donout, target_donors, true );
	donout.close();
	
	utility::io::ozstream accout(outfile+"_"+"acceptors.pdb.gz");
	::devel::scheme::dump_hbond_rays( accout, target_acceptors, false );
	accout.close();
			
	std::cout << "target_donors.size() " << target_donors.size() << " target_acceptors.size() " << target_acceptors.size() << std::endl;

	auto rot_tgt_scorer = std::make_shared< devel::scheme::ScoreRotamerVsTarget<
		VoxelArrayPtr, ::scheme::chemical::HBondRay, ::devel::scheme::RotamerIndex
		> >();
	{
		rot_tgt_scorer->rot_index_p_ = rot_index_p;
		rot_tgt_scorer->target_field_by_atype_ = field_by_atype;
		rot_tgt_scorer->target_donors_ = target_donors;
		rot_tgt_scorer->target_acceptors_ = target_acceptors;
		rot_tgt_scorer->hbond_weight_ = option[rifgen::hbond_weight]();
		rot_tgt_scorer->upweight_multi_hbond_ = option[rifgen::upweight_multi_hbond]() || option[ rifgen::dump_bidentate_hbonds ]();
		rot_tgt_scorer->upweight_iface_ = 1.0;
		rot_tgt_scorer->min_hb_quality_for_satisfaction_ = option[rifgen::min_hb_quality_for_satisfaction]();
        rot_tgt_scorer->long_hbond_fudge_distance_ = option[rifgen::long_hbond_fudge_distance]();
#ifdef USEGRIDSCORE
		rot_tgt_scorer->grid_scorer_ = grid_scorer;
		rot_tgt_scorer->soft_grid_energies_ = option[rifgen::soft_rosetta_grid_energies]();
#endif
        shared_ptr<DonorAcceptorCache> target_donor_cache, target_acceptor_cache;
        prepare_donor_acceptor_cache( target_donors, target_acceptors, *rot_tgt_scorer, target_donor_cache, target_acceptor_cache );

        rot_tgt_scorer->target_donor_cache_ = target_donor_cache;
        rot_tgt_scorer->target_acceptor_cache_ = target_acceptor_cache;

        rot_tgt_scorer->target_donor_names = target_donor_names;
        rot_tgt_scorer->target_acceptor_names = target_acceptor_names;

	}



		///////////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////// rif generation happens here! ///////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////
		auto params = make_shared<::devel::scheme::rif::RifGenParams>();
	  	params->target = target;
		params->target_tag = target_tag;
		params->output_prefix = outfile+"_";
		params->target_res = target_res;
		params->rot_index_p = rot_index_p;
		params->cache_data_path = cache_data_path;
		params->field_by_atype = field_by_atype;
		params->hbopt = hbopt;
#ifdef USEGRIDSCORE
		params->grid_scorer = grid_scorer;
#endif
		params->tuning_file = option[rifgen::tuning_file]();
		params->rot_tgt_scorer = rot_tgt_scorer;

		for( int igen = 0; igen < generators.size(); ++igen )
		{
			//cache the input 
			generators[igen]->generate_rif( rif_accum, params );
		}
		std::cout << "RifGenerators done" << std::endl;


		uint64_t N_motifs_found = rif_accum->n_motifs_found();
		// N_motifs_found += rif_accum->total_samples();
		std::cout << "RIFAccumulator building rif...." << std::endl;
		rif_accum->condense();
		rif = rif_accum->rif();
		// rif->set_xmap_ptr( rif_accum.rif_ );
		rif_accum->clear();

		cout << "RIF: " << " non0 in RIF: " << KMGT(rif->size()) << " N_motifs_found: "
			  << KMGT(N_motifs_found) << " coverage: " << (double)N_motifs_found/rif->size() << ", mem_use: " << KMGT(rif->mem_use()) << endl;

		cout << "RIF size: " << KMGT( rif->mem_use() ) << " load: " << rif->load_factor()
		     << ", sizeof(value_type) " << rif->sizeof_value_type() << endl;

		cout << "sorting rotamers in each hash entry" << endl;
		rif->finalize_rif();
		// __gnu_parallel::for_each( rif.map_.begin(), rif.map_.end(), call_sort_rotamers<XMap::Map::value_type> );
		// __gnu_parallel::for_each( rif.map_.begin(), rif.map_.end(), assert_is_sorted  <XMap::Map::value_type> );


		cout << "RIF " 
		     << " non0 in RIF: " << KMGT(rif->size()) 
		     << " N_motifs_found: " << KMGT(N_motifs_found) 
		     << " coverage: " << N_motifs_found*1.f/rif->size() << endl;

		rif->collision_analysis(cout);

		// save rif
		rif->print(std::cout);
		if( option[rifgen::outfile].user() ){
			cout << "before resize, RIF size: " << KMGT( rif->mem_use() ) << " load: " << rif->load_factor()
				  << ", sizeof(value_type) " << rif->sizeof_value_type() << endl;
			 cout << "NOT RESIZING RIF" << endl;
			// rif.map_.max_load_factor(0.8);
			// rif.map_.min_load_factor(0.4);
			// rif.map_.resize(0);
			// cout << "after  resize, RIF size: " << KMGT( rif.mem_use() ) << " load: " << rif.map_.size()*1.f/rif.map_.bucket_count() << endl;

			std::string fname = outfile;
			cout << "dumping rif to: " << fname << endl;

			std::string description = "from Will's rifgen app\n";
			description += "rotamer set size : " + str(rot_index_p->size())+"\n";
			description += "rotamer nprimary : " + str(rot_index_p->n_primary_rotamers())+"\n";
			description += "          target : " + (std::string)option[rifgen::target]()+"\n";
			description += "      target_res : " ; BOOST_FOREACH( int ir, target_res ) description += str(ir)+" "; description += "\n";
			description += "          apores : " ; BOOST_FOREACH( std::string s, option[rifgen::apores]() ) description += s+" "; description += "\n";
			description += "          donres : " ; BOOST_FOREACH( std::string s, option[rifgen::donres]() ) description += s+" "; description += "\n";
			description += "          accres : " ; BOOST_FOREACH( std::string s, option[rifgen::accres]() ) description += s+" "; description += "\n";
			description += "       beam_size : " + KMGT(option[rifgen::beam_size_M]()*1000000)+"\n";
			description += "    value stored : " + rif->value_name()+"\n";
			description += "apo_search_resls : " ; BOOST_FOREACH( float r, RESLS ) description += str(r)+" "; description += "\n";
			description += " score_cut_adjust..............." + str(option[rifgen::score_cut_adjust]())+"\n";
			description += " score_threshold................" + str(option[rifgen::score_threshold]())+"\n";
			description += " hbond_weight..................." + str(option[rifgen::hbond_weight]())+"\n";
			description += " upweight_multi_hbond..........." + str(option[rifgen::upweight_multi_hbond]())+"\n";
			description += " tip_tol_deg...................." + str(option[rifgen::tip_tol_deg]())+"\n";
			description += " rot_samp_range................." + str(option[rifgen::rot_samp_range]())+"\n";
			description += " rot_samp_resl.................." + str(option[rifgen::rot_samp_resl]())+"\n";
			description += " hbond_cart_sample_hack_range..." + str(option[rifgen::hbond_cart_sample_hack_range]())+"\n";
			description += " hbond_cart_sample_hack_resl...." + str(option[rifgen::hbond_cart_sample_hack_resl]())+"\n";
			description += " rosetta_field_resl............." + str(option[rifgen::rosetta_field_resl]())+"\n";
			description += " max_rf_bounding_ratio.........." + str(option[rifgen::max_rf_bounding_ratio]())+"\n";
			description += " rf_oversample.................." + str(option[rifgen::rf_oversample]())+"\n";
			description += " hash_cart_resl................." + str(option[rifgen::hash_cart_resl]())+"\n";
			description += " hash_angle_resl................" + str(option[rifgen::hash_angle_resl]())+"\n";
		    description += "       RIF cells : " + KMGT(rif->size()) + "\n";
		    description += "  Nrots inserted : " + KMGT(N_motifs_found) + "\n";
		    description += "        coverage : " + str( N_motifs_found*1.f/rif->size()) + "\n";

			std::cout << "===================================== rif description ======================================" << std::endl;
			std::cout << description << std::endl;
			std::cout << "============================================================================================" << std::endl;
			std::cout << "writing rif file " << fname << std::endl;



			// make bounding grids
			#ifdef USE_OPENMP
			#pragma omp parallel for schedule(dynamic,1)
			#endif
			for( int ibound = 0; ibound <= option[rifgen::lever_bounds]().size(); ++ibound ){
				if( ibound == 0 ){
					utility::io::ozstream out( fname , std::ios::binary );
					rif->save( out, description );
					out.close();
				} else {
					std::string bgfn = make_bounding_grids( rif_factory, rif, description, fname, ibound );
					#ifdef USE_OPENMP
					#pragma omp critical
					#endif
					bounding_grid_fnames.push_back( bgfn );
				}
			}

			std::cout << "done writing" << std::endl;

	} // end if not outfile exists

	std::string a_test_struct_fname("");
	if( option[rifgen::test_structures]().size() ){

		// Objective objective;

		for( int itest = 1; itest <= option[rifgen::test_structures]().size(); ++itest ){
			std::string const & testfile( option[rifgen::test_structures]()[itest] );
			std::cout << "TEST RIF " << outfile << " ON " << testfile << std::endl;
			core::pose::Pose test;
			core::import_pose::pose_from_file( test, testfile );
			{
				for( int ir = 1; ir <= test.size(); ++ir ){
					for( int ia = 1; ia <= test.residue_type(ir).natoms(); ++ia ){
						core::id::AtomID aid(ia,ir);
						test.set_xyz( aid, test.xyz( aid ) - target_center );
					}
				}
			}
			std::string tmp = utility::file_basename( testfile );
			a_test_struct_fname = outfile+"__ALIGNED__"+tmp;
			test.dump_pdb( a_test_struct_fname );

			std::vector<std::vector<float> > onebody_rotamer_energies; {
				utility::vector1<core::Size> test_res;
				for( int i = 1; i <= test.size(); ++i) test_res.push_back(i);
				std::string cachefile = "__1BE_" + utility::file_basename( testfile ) + (replace_all_with_ala_1bre?"_ALLALA":"") + ".bin.gz";
				get_onebody_rotamer_energies( test, test_res, rot_index, onebody_rotamer_energies, cache_data_path, cachefile, replace_all_with_ala_1bre, 1 );
			}

			typedef std::pair<int,Vec> ClashCrd;
			std::vector< ClashCrd > clash_coords;
			for( int ir = 1; ir <= test.size(); ++ir ){ // very hacky clash_dis check
				if( ! test.residue(ir).is_protein() ) continue;
				if( test.residue(ir).has("CA") ) clash_coords.push_back( std::make_pair( ir, test.residue(ir).xyz("CA") ) );
				if( test.residue(ir).has("C" ) ) clash_coords.push_back( std::make_pair( ir, test.residue(ir).xyz("C" ) ) );
				if( test.residue(ir).has("CB") ) clash_coords.push_back( std::make_pair( ir, test.residue(ir).xyz("CB") ) );
			}

			// std::cout << "dump test results to " << resultfile << std::endl;
			for( int ir = 1; ir <= test.size(); ++ir){

				// score native residue
				// {
				// 	Scene tmpscene(2);
				// 	utility::vector1<core::Size> resids(1,ir);
				// 	std::vector<SchemeAtom> atoms;
				// 	get_scheme_atoms( test, resids, atoms );
				// 	int restype = rot_index.chem_index_.resname2num_[test.residue(ir).name3()];
				// 	for( int ia = 0; ia < atoms.size(); ++ia){
				// 		SchemeAtom const & a( atoms[ia] );
				// 		if( a.type() >= 21 ) continue;
				// 		SceneAtom sa( a.position(), a.type(), restype, ia );
				// 		// runtime_assert( rot_index.chem_index_.atom_data( restype, ia ) == a.data() ); // why does this fail??
				// 		tmpscene.add_actor(1,sa);
				// 	}
				// 	tmpscene.add_actor( 0, VoxelActor( bounding_by_atype ) );
				// 	float score0 = objective( tmpscene, 0 ).template get<VoxelScore>();
				// 	if( score0 != 0.0 ){
				// 		std::cout << testfile << " resi: " << I(3,ir) << " resn: " << test.residue(ir).name3()
				// 									 << " restype: " << I(2,restype) << " apo scores:";
				// 		for( int r = 0; r < RESLS.size(); ++r ){
				// 			float score = objective( tmpscene, r ).template get<VoxelScore>();
				// 			std::cout << " " << F(6,3,score);
				// 		}
				// 		std::cout << endl;
				// 	}
				// }



				Eigen::Vector3f N ( test.residue(ir).xyz("N" ).x(), test.residue(ir).xyz("N" ).y(), test.residue(ir).xyz("N" ).z() );
				Eigen::Vector3f CA( test.residue(ir).xyz("CA").x(), test.residue(ir).xyz("CA").y(), test.residue(ir).xyz("CA").z() );
				Eigen::Vector3f C ( test.residue(ir).xyz("C" ).x(), test.residue(ir).xyz("C" ).y(), test.residue(ir).xyz("C" ).z() );
				::scheme::actor::BackboneActor<EigenXform> bbactor( N, CA , C );

				std::cout << "rif analysis of test structure disabled! " << ir << std::endl;

				// XMap::Key key = rif.hasher_.get_key( bbactor.position_ );
				// XMapVal val = rif[key];
				// // cout << ir << " " << key << endl;
				// if( val.size() ) cout << testfile << " " << ir << " " << val << endl;
				// for(int irot = 0; irot < val.size(); ++irot){

				// 	float obe = onebody_rotamer_energies[ ir-1 ][ val.rotamer(irot) ];

				// 	double clash_dis = 9e9;
				// 	// loop over rot atoms, starting after CB (which should be #3)
				// 	for( int ia = 4; ia < rot_index.rotamers_[val.rotamer(irot)].nheavyatoms; ++ia ){
				// 		SchemeAtom a = rot_index.rotamers_[val.rotamer(irot)].atoms_[ia];
				// 		a.set_position( bbactor.position() * a.position() ); // is copy
				// 		Vec xyz( a.position()[0], a.position()[1], a.position()[2] );
				// 		BOOST_FOREACH( ClashCrd const & clash_coord, clash_coords ){
				// 			if( std::abs( clash_coord.first-ir ) < 1 ) continue; // skip if same res or adjcent
				// 			double d2 = xyz.distance_squared( clash_coord.second );
				// 			// if( d2 < clash_dis && d2 < 9.0 ) std::cout << clash_coord.first << " " << sqrt(d2) << endl;
				// 			clash_dis = std::min( clash_dis, d2 );
				// 		}
				// 	}
				// 	clash_dis = sqrt( clash_dis );

				// 	cout << testfile << " " << I(4,ir) << " " << F(7,3,val.score(irot)) << " " << rot_index.resname(val.rotamer(irot)) << I(3,val.rotamer(irot))
				// 		  << " " << F(7,3,clash_dis) << " " << F(7,2,obe) << endl;

				// 	if( obe + val.score(irot) >= -0.5 //&& rot_index.resname(val.rotamer(irot)) != test.residue(ir).name3()
				// 	) continue;

				// 	utility::io::ozstream out( std::string("test_hit" )
				// 		+"_"+str( ir, 3 )
				// 		+"_"+rot_index.resname(val.rotamer(irot))
				// 		+"_"+str( val.rotamer(irot), 3 )
				// 		+".pdb"
				// 	);
				// 	BOOST_FOREACH( SchemeAtom a, rot_index.rotamers_[val.rotamer(irot)].atoms_ ){
				// 		a.set_position( bbactor.position() * a.position() ); // is copy
				// 		::scheme::actor::write_pdb( out, a, rot_index.chem_index_ );
				// 	}
				// 	out.close();

				// }


			}

		}
	}

	for( auto & vv : bounding_by_atype ) for( auto vp : vv ) delete vp;

	std::cout << "NOT DELETING VoxelArrayPtr's, should use smart ptrs!" << std::endl;
	// BOOST_FOREACH( VoxelArrayPtr vp, field_by_atype ){ // taken care of as part of the above
	// 	delete vp;
	// }

	omp_destroy_lock( & cout_lock ) ;
	omp_destroy_lock( & io_lock );
	omp_destroy_lock( & pose_lock );
	omp_destroy_lock( & hbond_geoms_cache_lock );

	// This function is mirrored in rif_dock_test.cc
	if ( option[rifgen::dump_rifgen_hdf5]() ) {
		core::pose::Pose all_rots;
		for ( int irot = 0; irot < rot_index.size(); irot++ ) {
			std::string lresname;
			if (rot_index.d_l_map_.find(rot_index_spec.resname(irot)) != rot_index.d_l_map_.end()) { lresname = rot_index.d_l_map_.find(rot_index_spec.resname(irot))->second; }
			all_rots.append_residue_by_jump( *rot_index_spec.get_rotamer_at_identity( irot, lresname ), 1 );
		}

		all_rots.pdb_info( std::make_shared<core::pose::PDBInfo>( all_rots ) );
		for ( core::Size i = 1; i <= all_rots.size(); i++ ) {
			all_rots.pdb_info()->chain( i, 'A' );
		}
		std::cout << "Dumping rif standard rotamers to rif_rotamers.pdb" << std::endl;
		all_rots.dump_pdb("rif_rotamers.pdb");

		rif->dump_rif_to_hdf5();

	}


	std::cout << "rif_hier_DONE" << std::endl;
	std::sort( bounding_grid_fnames.begin(), bounding_grid_fnames.end() );
	std::reverse( bounding_grid_fnames.begin(), bounding_grid_fnames.end() );
	std::cout << "########################################### what you need for docking ###########################################" << std::endl;
		std::cout << "-rif_dock:target_pdb            " << centered_target_pdbfile << std::endl;
	if( option[basic::options::OptionKeys::in::file::extra_res_fa].size() == 1 )
		std::cout << "-in:file:extra_res_fa           " << option[basic::options::OptionKeys::in::file::extra_res_fa]()[1] << std::endl;
	if( target_reslist_file.size() )
		std::cout << "-rif_dock:target_res            " << target_reslist_file << std::endl;
	std::cout <<     "-rif_dock:target_rf_resl        " << rf_resl << std::endl;
	std::cout <<     "-rif_dock:target_rf_cache       " << fname_grids_for_docking << std::endl;
	for( auto s : bounding_grid_fnames )
		std::cout << "-rif_dock:target_bounding_xmaps " << s << std::endl;
	std::cout <<     "-rif_dock:target_rif            " << outfile << std::endl;
	if ( needs_donors_acceptors ) {
		std::cout << "-rif_dock:target_donors         " << params->output_prefix + "donors.pdb.gz" << std::endl;
		std::cout << "-rif_dock:target_acceptors      " << params->output_prefix + "acceptors.pdb.gz" << std::endl;
	}
	std::cout <<     "-rif_dock:extra_rotamers        " << option[rifgen::extra_rotamers]() << std::endl;
	std::cout <<     "-rif_dock:extra_rif_rotamers    " << option[rifgen::extra_rif_rotamers]() << std::endl;
	std::cout <<     "-rif_dock:rot_spec_fname        " << rot_spec_fname << std::endl;
	std::cout << "#################################################################################################################" << std::endl;

	return 0;
 }

