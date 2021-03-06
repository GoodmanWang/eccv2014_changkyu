
#define NUM_OF_PARTS_CAR (6)
#define CONV_RADIUS(x)        ((x)*(M_PI)/180)
#define CHECK_RANGE_DEGREE(x) ( ((int)(x) - (int)((x) / 360) * 360 + 360) % 360 + ((x)-(int)(x)) )

typedef enum ENUM_OBJECT_CATEGORY
{
	OBJECT_CATEGORY_CAR=1,
	OBJECT_CATEGORY_CHAIR,
	OBJECT_CATEGORY_TABLE,
} ENUM_OBJECT_CATEGORY;

/****************************************
 * MVT Parameter
 ****************************************/
typedef struct MVT_Param
{
	/* Input Image */
	std::string path_imgs;
	std::string filepath_imglist;
	std::string namefmt_imgs;

	std::string path_dpmconfs;
	std::string namefmt_dpmconfs;

	unsigned int idx_img_start;
	unsigned int idx_img_end;

	unsigned int width_img;
	unsigned int height_img;

	/* Result File */
	std::string filepath_result;
	std::string filepath_log;

	/* Tracker Options */
	bool use_dpm;
	bool use_alm;
	bool use_mil;
	bool use_pairwise;
	bool use_prior;
	bool use_mil_root;
	bool is_vis;

	bool use_init;
	double init_state_x;
	double init_state_y;
	double init_state_a;
	double init_state_e;
	double init_state_d;

	/* Target Object Information */
	ENUM_OBJECT_CATEGORY object_category;
	std::string filepath_3dobject_model;

	/* Sampling Information */
	unsigned int num_of_viewpoint_sample;
	unsigned int num_of_center_sample;
	unsigned int num_of_partcenter_sample;

	double std_azimuth;
	double std_elevation;
	double std_distance;

	double std_prior_azimuth;
	double std_prior_elevation;
	double std_prior_distance;

	double thresh_dpm;
	double thresh2_dpm;
	double thresh_alm;
	double thresh_mil;

	double weight_mil_root;
	unsigned int height_mil_root;

	unsigned int srchwinsz;

	cv::ObjectTrackerParams param_mil;
} MVT_Param;

/****************************************
 * MVT 2D Part
 ****************************************/
typedef struct MVT_2D_Part
{
	std::vector<cv::Point2d> vertices;
	cv::Point2d center;

} MVT_2D_Part;

typedef struct MVT_2D_Part_Front
{
	std::vector<cv::Point2d> vertices;
	cv::Point2d center;

	double width;
	double height;
	double distance;

	double viewport;

	std::string name;

} MVT_2D_Part_Front;

/****************************************
 * MVT 3D Part
 ****************************************/
typedef struct MVT_3D_Part
{
	std::vector<cv::Point3d> vertices;
	double plane[4];
	cv::Point3d center;
	cv::Point3d xaxis;
	cv::Point3d yaxis;
} MVT_3D_Part;

typedef std::vector<cv::Point2d> MVT_Point_Set;

/****************************************
 * MVT Viewpoint Information
 ****************************************/
typedef unsigned int   MVT_AZIMUTH_DISC;
typedef unsigned int MVT_ELEVATION_DISC;
typedef unsigned int  MVT_DISTANCE_DISC;

typedef double   MVT_AZIMUTH;
typedef double MVT_ELEVATION;
typedef double  MVT_DISTANCE;

typedef struct MVT_Viewpoint
{
	MVT_AZIMUTH   azimuth;    /* Azimuth   */
	MVT_ELEVATION elevation;  /* Elevation */
	MVT_DISTANCE  distance;   /* Distance  */
} MVT_Viewpoint;

typedef struct MVT_Viewpoint_IDX
{
	unsigned int a;
	unsigned int e;
	unsigned int d;
} MVT_Viewpoint_IDX;

/****************************************
 * MVT Sampling
 ****************************************/
typedef struct MVT_STD_Sampling
{
	double std_azimuth;
	double std_elevation;
	double std_distance;

	std::vector<double> std_x_partsNroots;
	std::vector<double> std_y_partsNroots;
} MVT_STD_Sampling;

typedef class MVT_2D_Object MVT_2D_Object;
typedef class MVT_3D_Object MVT_3D_Object;
typedef class MVT_SampleSet MVT_SampleSet;

/****************************************
 * MVT State
 ****************************************/

typedef enum ENUM_MVT_LIKELIHOOD_ROOT
{
	MVT_LIKELIHOOD_DPM=0,
	MVT_LIKELIHOOD_MIL_ROOT,
	NUM_OF_LIKELIHOOD_TYPE_ROOT
} ENUM_MVT_LIKELIHOOD_ROOT;

typedef enum ENUM_MVT_LIKELIHOOD_PARTSNROOTS
{
	MVT_LIKELIHOOD_ALM=0,
	MVT_LIKELIHOOD_MIL,
	NUM_OF_LIKELIHOOD_TYPE_PARTSNROOTS,
} ENUM_MVT_LIKELIHOOD_PARTSNROOTS;

typedef enum ENUM_MVT_MOTION
{
	MVT_MOTION_PRIOR=0,
	MVT_MOTION_PAIRWISE,
	NUM_OF_MOTION
} ENUM_MVT_MOTION;

typedef struct MVT_Potential
{
	bool   is_requested;
	bool   is_updated;
	double value;
} MVT_Potential;

typedef struct MVT_State
{
	int            idx_viewpoint;
	MVT_Viewpoint  viewpoint;
	MVT_2D_Object* pObject2d;

	cv::Rect     bbox_root;
	cv::Rect     bbox_partsNroots;

	cv::Point2d* centers;
	cv::Point*   centers_rectified;

	cv::Point2d center_root;

	double potential;
	double potential_local;
	double potential_local_online;

	double likelihood_global;
	double overlap;
	double likelihood_root_all;
	double likelihood_root[NUM_OF_LIKELIHOOD_TYPE_ROOT];
	double likelihood_root_local[NUM_OF_LIKELIHOOD_TYPE_ROOT];
	double likelihood_root_global[NUM_OF_LIKELIHOOD_TYPE_ROOT];
	double likelihood_partsNroots_all;
	double likelihood_partsNroots[NUM_OF_LIKELIHOOD_TYPE_PARTSNROOTS];
	double likelihood_partsNroots_local[NUM_OF_LIKELIHOOD_TYPE_PARTSNROOTS];
	double likelihood_partsNroots_global[NUM_OF_LIKELIHOOD_TYPE_PARTSNROOTS];
	double* likelihood_partsNroots_pr[NUM_OF_LIKELIHOOD_TYPE_PARTSNROOTS];
	double* likelihood_partsNroots_pr_global[NUM_OF_LIKELIHOOD_TYPE_PARTSNROOTS];

	double motion_prior;
	double motion_pairwise;

} MVT_State;

typedef struct MVT_Image
{
	cv::Mat*  pImage;
	CUMATRIX cumx_image;
	char*    filepath_conf;
} MVT_Image;

typedef std::vector<MVT_State> MVT_State_Set;
