#include <string.h>
#include "mvt.h"
#include <math.h>

using namespace boost::math;

namespace mvt
{

void get_Confidence(const mxArray* model_, double x, double y, double* &dets, unsigned int &n_dets );

DetectorDPM::DetectorDPM(MVT_Param &param)
	: MVT_Potential_Model_Root()
{
	m_confidence_outofimage = -INFINITY;//(float)param.thresh_dpm;
	m_mat   = NULL;
	m_model = NULL;

	m_x_min = 0;
	m_y_min = 0;

	m_nd = new boost::math::normal(0.0, 1.0);
	m_pdf_mean = pdf(*m_nd,0.0);
}

DetectorDPM::~DetectorDPM()
{
	if(m_mat) matClose(m_mat);
	if( m_model ) mxDestroyArray(m_model);
}

void DetectorDPM::SetConf(char* filepath_conf)
{
	if(m_mat) matClose(m_mat);

	m_mat = matOpen(filepath_conf,"r");
	if( m_mat != NULL )
	{
		if( m_model ) mxDestroyArray(m_model);
		m_model = matGetVariable(m_mat, "model_score");
	}
}

void DetectorDPM::SetImage(cv::Mat *pImage)
{
	m_x_max = (double)pImage->cols-1;
	m_y_max = (double)pImage->rows-1;
}

float DetectorDPM::GetPotential(MVT_State* p_states)
{
	double *detections = NULL;
	unsigned int n_detections;
	get_Confidence(m_model, p_states->bbox_root.x, p_states->bbox_root.y, detections, n_detections);

	if( detections )
	{
		double value_max = -INFINITY;
		unsigned int idx_max = -1;
		double* res = NULL;
		for( unsigned int d=0; d<n_detections; d++ )
		{
			res = &(detections[d]);

			double value = 1;
#if 1 //ORG0 seq1
			value *= pdf(*m_nd, (p_states->bbox_root.width -res[n_detections*2])/(p_states->bbox_root.width *6) ) / m_pdf_mean;
			value *= pdf(*m_nd, (p_states->bbox_root.height-res[n_detections*3])/(p_states->bbox_root.height*6) ) / m_pdf_mean;
#endif

			if( value_max < exp(res[n_detections*5])*value )
			{
				value_max = exp(res[n_detections*5])*value;
				idx_max = d;
			}
		}
		if( idx_max == -1 )
		{
			p_states->likelihood_root[MVT_LIKELIHOOD_DPM] = -INFINITY;
		}
		else
		{
			res = &(detections[idx_max]);
			p_states->bbox_root.x = res[0];
			p_states->bbox_root.y = res[n_detections];
			p_states->likelihood_root[MVT_LIKELIHOOD_DPM] = exp(res[n_detections*5]);
			if( g_param.use_mil_root==false || p_states->likelihood_root[MVT_LIKELIHOOD_DPM] > g_param.thresh2_dpm )
			{
				p_states->bbox_root.width  = res[n_detections*2] - p_states->bbox_root.x;
				p_states->bbox_root.height = res[n_detections*3] - p_states->bbox_root.y;
			}
		}

		free(detections);
	}
	return (float)p_states->likelihood_root[MVT_LIKELIHOOD_DPM];
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////

/* Obtain the confidence score from DPM */

enum output_fields {
  DET_USE = 0,   // current symbol is used
  DET_IND,       // rule index
  DET_X,         // x coord (filter and deformation)
  DET_Y,         // y coord (filter and deformation)
  DET_L,         // level (filter)
  DET_DS,        // # of 2x scalings relative to the start symbol location
  DET_PX,        // x coord of "probe" (deformation)
  DET_PY,        // y coord of "probe" (deformation)
  DET_VAL,       // score of current symbol
  DET_SZ         // <count number of constants above>
};

struct node {
  int symbol;   // grammar symbol
  int x;        // x location for symbol
  int y;        // y location for symbol
  int l;        // scale level for symbol
  int ds;       // # of 2x scalings relative to the start symbol location
  double val;   // score for symbol
};

static const mxArray *model = NULL;
static const mxArray *rules = NULL;
static node *Q = NULL;
static int start_symbol = 0;
static int interval = 0;

static inline double min(double x, double y) { return (x <= y ? x : y); }
static inline double max(double x, double y) { return (x <= y ? y : x); }
static inline int pow2(int p) { return (1<<p); }

// Compute amount of virtual padding needed to align pyramid
// levels with 2*ds scale separation.
static inline int virtpadding(int padding, int ds) {
  // subtract one because each level already has a one
  // padding wide border around it
  return padding*(pow2(ds)-1);
}


// push a symbol onto the stack
static inline void push(const node& n, int& cur, int padx, int pady,
                        int probex, int probey, int px, int py,
                        int pl, int ds, int r, const double *rhs,
                        int rhsind) {
  // acccumulate # of 2x rescalings
  int pds = n.ds + ds;
  // symbol to push onto the stack
  int ps = (int)rhs[rhsind]-1;

  // locate score (or set to zero if the symbol is hallucinated beyond
  // the feature pyramid boundary)
  mxArray *mxScore = mxGetCell(mxGetField(mxGetField(model, 0, "symbols"),
                                          ps, "score"), pl);
  double *score = mxGetPr(mxScore);
  const mwSize *sz = mxGetDimensions(mxScore);
  double pval = score[probex*sz[0] + probey];
  // push symbol @ (px,py,pl) with score pval onto the stack
  cur++;
  Q[cur].symbol = ps;
  Q[cur].x = px;
  Q[cur].y = py;
  Q[cur].l = pl;
  Q[cur].ds = pds;
  Q[cur].val = pval;
}

// trace a single detection
static void trace(int padx, int pady, const double *scales,
                  int sx, int sy, int sl, double sval,
                  double *dets, mwSize *detsdim ) {
  // initial stack for tracing the detection
  int cur = 0;
  Q[cur].symbol = start_symbol;
  Q[cur].x = sx;
  Q[cur].y = sy;
  Q[cur].l = sl;
  Q[cur].ds = 0;
  Q[cur].val = sval;

  while (cur >= 0) {
    // pop a node off the stack
    const node n = Q[cur];
    cur--;

    mxChar type = mxGetChars(mxGetField(mxGetField(model, 0, "symbols"), n.symbol, "type"))[0];
    // symbol is a terminal
    if (type == 'T') {

      // terminal symbol
      int fi = (int)mxGetScalar(mxGetField(mxGetField(model, 0, "symbols"), n.symbol, "filter")) - 1;
      // filter size
      double *fsz = mxGetPr(mxGetField(mxGetField(model, 0, "filters"), fi, "size"));
      // detection scale
      double scale = mxGetScalar(mxGetField(model, 0, "sbin"))/scales[n.l];

      // compute and record image coordinates for the filter
      double x1 = (n.x-padx*pow2(n.ds))*scale;
      double y1 = (n.y-pady*pow2(n.ds))*scale;
      double x2 = x1 + fsz[1]*scale - 1;
      double y2 = y1 + fsz[0]*scale - 1;

      continue;
    }

    // find the rule that produced the current node by looking at
    // which score table holds n.val at the symbol's location
    const mxArray *symrules = mxGetCell(rules, n.symbol);
    const mwSize *rulesdim = mxGetDimensions(symrules);
    unsigned int r = 0;
    for (; r < rulesdim[1]; r++) {
      // probe location = symbol location minus virtual padding
      int probey = n.y-virtpadding(pady, n.ds);
      int probex = n.x-virtpadding(padx, n.ds);
      mxArray *mxScore = mxGetCell(mxGetField(symrules, r, "score"), n.l);
      const double *score = mxGetPr(mxScore);
      const mwSize *sz = mxGetDimensions(mxScore);

      // pick this rule if the score at the probe location matches n.val
      if (score[probex*sz[0] + probey] == n.val) {
        break;
      }
    }

    // record a detection window for the start symbol
    if (n.symbol == start_symbol) {
      // get detection window for start_symbol and rule r
      mxArray *mxdetwin = mxGetField(symrules, r, "detwindow");
      double *detwin = mxGetPr(mxdetwin);

      // detection scale
      double scale = mxGetScalar(mxGetField(model, 0, "sbin"))/scales[n.l];

      // compute and record image coordinates of the detection window
      double x1 = (n.x-padx*pow2(n.ds))*scale;
      double y1 = (n.y-pady*pow2(n.ds))*scale;
      double x2 = x1 + detwin[1]*scale - 1;
      double y2 = y1 + detwin[0]*scale - 1;

      dets[detsdim[0]*0] = x1;
      dets[detsdim[0]*1] = y1;
      dets[detsdim[0]*2] = x2;
      dets[detsdim[0]*3] = y2;
      dets[detsdim[0]*4] = r + 1;
      dets[detsdim[0]*5] = n.val;

      return;
    }

    // push rhs symbols from the selected rule
    type = mxGetChars(mxGetField(symrules, r, "type"))[0];
    const mxArray *mxrhs = mxGetField(symrules, r, "rhs");
    const mwSize *rhsdim = mxGetDimensions(mxrhs);
    const double *rhs = mxGetPr(mxrhs);
    if (type == 'S') {
      // structural rule
      for (unsigned int j = 0; j < rhsdim[1]; j++) {
        const double *anchor = mxGetPr(mxGetCell(mxGetField(symrules, r, "anchor"), j));
        int ax = (int)anchor[0];
        int ay = (int)anchor[1];
        int ds = (int)anchor[2];
        // compute location of the rhs symbol
        int px = n.x*pow2(ds) + ax;
        int py = n.y*pow2(ds) + ay;
        int pl = n.l - interval*ds;
        int probex = px - virtpadding(padx, n.ds+ds);
        // remove virtual padding for to compute the probe location in the
        // score table
        int probey = py - virtpadding(pady, n.ds+ds);
        push(n, cur, padx, pady, probex, probey, px, py, pl, ds, r, rhs, j);
      }
    } else {
      // deformation rule (only 1 rhs symbol)
      mxArray *mxIx = mxGetCell(mxGetField(symrules, r, "Ix"), n.l);
      mxArray *mxIy = mxGetCell(mxGetField(symrules, r, "Iy"), n.l);
      int *Ix = (int *)mxGetPr(mxIx);
      int *Iy = (int *)mxGetPr(mxIy);

      const mwSize *isz = mxGetDimensions(mxIx);
      int px = n.x;
      int py = n.y;
      // probe location for looking up displacement of rhs symbol
      int probex = n.x - virtpadding(padx, n.ds);
      int probey = n.y - virtpadding(pady, n.ds);
      // probe location for accessing the score of the rhs symbol
      int probex2 = probex;
      int probey2 = probey;
      // if the probe location is in the feature pyramid retrieve the
      // deformation location from Ix and Iy
      // subtract 1 because Ix/Iy use 1-based indexing
      px = Ix[probex*isz[0] + probey] - 1 + virtpadding(padx, n.ds);
      py = Iy[probex*isz[0] + probey] - 1 + virtpadding(pady, n.ds);
      // remove virtual padding for score look up
      probex2 = Ix[probex*isz[0] + probey] - 1;
      probey2 = Iy[probex*isz[0] + probey] - 1;
      push(n, cur, padx, pady, probex2, probey2, px, py, n.l, 0, r, rhs, 0);
    }
  }
}

/* model is the DPM after detection, where scores are stored */
/* x, y are the left corner coordinates of the bounding box */
/* usage: [dets, boxes, info] = get_confidence(model_score, x, y); */
void get_Confidence(const mxArray* model_, double x, double y, double* &detections, unsigned int &n_detections )
{
  model = model_;
  start_symbol = (int)mxGetScalar(mxGetField(model, 0, "start")) - 1;
  rules = mxGetField(model, 0, "rules");
  interval = (int)mxGetScalar(mxGetField(model, 0, "interval"));

  /* extract informtion from model */
  const int padx = (int)mxGetScalar(mxGetField(model, 0, "padx"));
  const int pady = (int)mxGetScalar(mxGetField(model, 0, "pady"));
  const int num_scale = (int)mxGetM(mxGetField(model, 0, "scales"));
  const int sbin = (int)mxGetScalar(mxGetField(model, 0, "sbin"));
  const double *scales = (double*)mxGetPr(mxGetField(model, 0, "scales"));
  int num = num_scale - interval;

  /* X, Y location, L level, S score */
  int *X = (int*)malloc(sizeof(int) * num);
  int *Y = (int*)malloc(sizeof(int) * num);
  int *L = (int*)malloc(sizeof(int) * num);
  double *S = (double*)malloc(sizeof(double) * num);

  /* loop for each scale */
  for(int i = 0; i < num; i++)
  {
    int l = i + interval;
    /* get detection score at the current scale */
    mxArray *mxScore = mxGetCell(mxGetField(mxGetField(model, 0, "symbols"), start_symbol, "score"), l);
    double *score = mxGetPr(mxScore);
    const mwSize *sz = mxGetDimensions(mxScore);

    /* change pixel to HOG cell location */
    int px = floor(x * scales[l] / sbin + padx);
    px = px < 0 ? 0 : px;
    px = px >= (int)sz[1] ? (int)sz[1] - 1 : px;
    X[i] = px;

    int py = floor(y * scales[l] / sbin + pady);
    py = py < 0 ? 0 : py;
    py = py >= (int)sz[0] ? (int)sz[0] - 1 : py;
    Y[i] = py;

    /* scale index and detection score */
    L[i] = l;
    S[i] = score[px*sz[0] + py];
  }

  const int numsymbols = (int)mxGetScalar(mxGetField(model, 0, "numsymbols"));
  Q = (node *)mxCalloc(numsymbols, sizeof(node));

  mwSize detsdim[2];
  detsdim[0] = num;
  detsdim[1] = 4+1+1;   // bounding box, component #, score

  detections = (double*)malloc(sizeof(double)*detsdim[0]*detsdim[1]);

  // trace detections and write output into out
  int count = 0;
  for (int i = 0; i < num; i++)
  {
    trace(padx, pady, scales, X[i], Y[i], L[i], S[i],
          &(detections[i]), detsdim);
    count++;
  }

  free(X);
  free(Y);
  free(L);
  free(S);

  // cleanup
  mxFree(Q);

  n_detections = num;
}

}
