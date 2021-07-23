/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
 
#ifndef __POSE_NET_H__
#define __POSE_NET_H__


#include "tensorNet.h"
#include <array>


/**
 * Name of default input blob for pose estimation ONNX model.
 * @ingroup poseNet
 */
#define POSENET_DEFAULT_INPUT   "input"

/**
 * Name of default output blob of the confidence map for pose estimation ONNX model.
 * @ingroup poseNet
 */
#define POSENET_DEFAULT_CMAP  "cmap"

/**
 * Name of default output blob of the Part Affinity Field (PAF) for pose estimation ONNX model.
 * @ingroup poseNet
 */
#define POSENET_DEFAULT_PAF  "paf"

/**
 * Default value of the minimum confidence threshold
 * @ingroup poseNet
 */
#define POSENET_DEFAULT_THRESHOLD 0.15f

/**
 * Default alpha blending value used during overlay
 * @ingroup poseNet
 */
//#define POSENET_DEFAULT_ALPHA 120

/**
 * Standard command-line options able to be passed to poseNet::Create()
 * @ingroup imageNet
 */
#define POSENET_USAGE_STRING  "poseNet arguments: \n" 								\
		  "  --network=NETWORK     pre-trained model to load, one of the following:\n" 		\
		  "                            * ssd-mobilenet-v1\n" 							\
		  "                            * ssd-mobilenet-v2 (default)\n" 					\
		  "                            * ssd-inception-v2\n" 							\
		  "                            * pednet\n" 									\
		  "                            * multiped\n" 								\
		  "                            * facenet\n" 									\
		  "                            * coco-airplane\n" 							\
		  "                            * coco-bottle\n" 								\
		  "                            * coco-chair\n" 								\
		  "                            * coco-dog\n" 								\
		  "  --model=MODEL         path to custom model to load (caffemodel, uff, or onnx)\n" 					\
		  "  --prototxt=PROTOTXT   path to custom prototxt to load (for .caffemodel only)\n" 					\
		  "  --labels=LABELS       path to text file containing the labels for each class\n" 					\
		  "  --input-blob=INPUT    name of the input layer (default is '" POSENET_DEFAULT_INPUT "')\n" 			\
		  "  --output-cvg=COVERAGE name of the coverge output layer (default is '" POSENET_DEFAULT_CMAP "')\n" 	\
		  "  --output-bbox=BOXES   name of the bounding output layer (default is '" POSENET_DEFAULT_PAF "')\n" 	\
		  "  --mean-pixel=PIXEL    mean pixel value to subtract from input (default is 0.0)\n"					\
		  "  --batch-size=BATCH    maximum batch size (default is 1)\n"										\
		  "  --threshold=THRESHOLD minimum threshold for detection (default is 0.5)\n"							\
            "  --alpha=ALPHA         overlay alpha blending value, range 0-255 (default: 120)\n"					\
		  "  --overlay=OVERLAY     detection overlay flags (e.g. --overlay=box,labels,conf)\n"					\
		  "                        valid combinations are:  'box', 'labels', 'conf', 'none'\n"					\
		  "  --profile             enable layer profiling in TensorRT\n\n"


/**
 * Pose estimation models with TensorRT support.
 * @ingroup poseNet
 */
class poseNet : public tensorNet
{
public:	
	/**
	 * The pose of an object, composed of links between keypoints.
	 * Each image can have multiple objects detected per frame.
	 */
	struct ObjectPose
	{		
		uint32_t ID;	/**< Object ID in the image frame, starting with 0 */
		
		float Left;	/**< Bounding box left, as determined by the left-most keypoint in the pose */
		float Right;	/**< Bounding box right, as determined by the right-most keypoint in the pose */
		float Top;	/**< Bounding box top, as determined by the top-most keypoint in the pose */
		float Bottom;	/**< Bounding box bottom, as determined by the bottom-most keypoint in the pose */
		
		/**
		 * A keypoint or joint in the topology. A link is formed between two keypoints.
		 */
		struct Keypoint
		{
			uint32_t ID;	/**< ID of the keypoint - the name can be retrieved with poseNet::GetKeypointName() */
			float x;		/**< The x coordinate of the keypoint */
			float y;		/**< The y coordinate of the keypoint */
		};
		
		std::vector<Keypoint> Keypoints;			/**< List of keypoints in the object, which contain the keypoint ID and x/y coordinates */
		std::vector<std::array<uint32_t, 2>> Links;	/**< List of links in the object.  Each link has two keypoint indexes into the Keypoints list */

		/**< Find a keypoint index by it's ID, or return -1 if not found.  This returns an index into the Keypoints list */
		inline int FindKeypoint(uint32_t id) const;         
		
		/**< Find a link index by two keypoint ID's, or return -1 if not found.  This returns an index into the Links list */
		inline int FindLink(uint32_t a, uint32_t b) const;  
	};
	
	/**
	 * Overlay flags (can be OR'd together).
	 */
	enum OverlayFlags
	{
		OVERLAY_NONE      = 0,			/**< No overlay. */
		OVERLAY_LINKS     = (1 << 0),		/**< Overlay the skeleton links (bones) as lines  */
		OVERLAY_KEYPOINTS = (1 << 1),		/**< Overlay the keypoints (joints) as circles */
		OVERLAY_DEFAULT   = OVERLAY_LINKS|OVERLAY_KEYPOINTS,
	};
	
	/**
	 * Network choice enumeration.
	 */
	enum NetworkType
	{
		CUSTOM = 0,		/**< Custom model from user */
		RESNET18_BODY,		/**< ResNet18-based human body model with PAF attention */
		RESNET18_HAND,		/**< ResNet18-based human hand model with PAF attention */
		DENSENET121_BODY,	/**< DenseNet121-based human body model with PAF attention */
	};

	/**
	 * Parse a string to one of the built-in pretrained models.
	 * @returns one of the poseNet::NetworkType enums, or poseNet::CUSTOM on invalid string.
	 */
	static NetworkType NetworkTypeFromStr( const char* model_name );

	/**
	 * Parse a string sequence into OverlayFlags enum.
	 * Valid flags are "none", "box", "label", and "conf" and it is possible to combine flags
	 * (bitwise OR) together with commas or pipe (|) symbol.  For example, the string sequence
	 * "box,label,conf" would return the flags `OVERLAY_BOX|OVERLAY_LABEL|OVERLAY_CONFIDENCE`.
	 */
	static uint32_t OverlayFlagsFromStr( const char* flags );

	/**
	 * Load a new network instance
	 * @param networkType type of pre-supported network to load
	 * @param threshold default minimum threshold for detection
	 * @param maxBatchSize The maximum batch size that the network will support and be optimized for.
	 */
	static poseNet* Create( NetworkType networkType=RESNET18_BODY, float threshold=POSENET_DEFAULT_THRESHOLD, 
					    uint32_t maxBatchSize=DEFAULT_MAX_BATCH_SIZE, precisionType precision=TYPE_FASTEST, 
					    deviceType device=DEVICE_GPU, bool allowGPUFallback=true );
	
	/**
	 * Load a custom network instance
	 * @param model_path File path to the ONNX model
	 * @param topology File path to the topology JSON
	 * @param threshold default minimum confidence thrshold
	 * @param input Name of the input layer blob.
	 * @param cmap Name of the output confidence map layer.
	 * @param paf Name of the output Part Affinity Field (PAF) layer.
	 * @param maxBatchSize The maximum batch size that the network will support and be optimized for.
	 */
	static poseNet* Create( const char* model_path, const char* topology, 
					    float threshold=POSENET_DEFAULT_THRESHOLD, 
					    const char* input = POSENET_DEFAULT_INPUT, 
					    const char* cmap = POSENET_DEFAULT_CMAP, 
					    const char* paf = POSENET_DEFAULT_PAF,
					    uint32_t maxBatchSize=DEFAULT_MAX_BATCH_SIZE, 
					    precisionType precision=TYPE_FASTEST,
				   	    deviceType device=DEVICE_GPU, bool allowGPUFallback=true );

	/**
	 * Load a new network instance by parsing the command line.
	 */
	static poseNet* Create( int argc, char** argv );
	
	/**
	 * Load a new network instance by parsing the command line.
	 */
	static poseNet* Create( const commandLine& cmdLine );
	
	/**
	 * Usage string for command line arguments to Create()
	 */
	static inline const char* Usage() 		{ return POSENET_USAGE_STRING; }

	/**
	 * Destory
	 */
	virtual ~poseNet();
	
	/**
	 * Perform pose estimation on the given image, returning object poses, and overlay the results.
	 * @param[in]  image input image in CUDA device memory (uchar3/uchar4/float3/float4)
	 * @param[in]  width width of the input image in pixels.
	 * @param[in]  height height of the input image in pixels.
	 * @param[out] poses array of ObjectPose structs that will be filled for each detected object.
	 * @param[in]  overlay bitwise OR combination of overlay flags (@see OverlayFlags and @see Overlay()), or OVERLAY_NONE.
	 * @returns    True on success, or false if an error occurred.
	 */
	template<typename T> bool Process( T* image, uint32_t width, uint32_t height, std::vector<ObjectPose>& poses, uint32_t overlay=OVERLAY_DEFAULT )					{ return Process((void*)image, width, height, imageFormatFromType<T>(), poses, overlay); }

	/**
	 * Perform pose estimation on the given image, and overlay the results.
	 * @param[in]  image input image in CUDA device memory (uchar3/uchar4/float3/float4)
	 * @param[in]  width width of the input image in pixels.
	 * @param[in]  height height of the input image in pixels.
	 * @param[out] poses array of ObjectPose structs that will be filled for each detected object.
	 * @param[in]  overlay bitwise OR combination of overlay flags (@see OverlayFlags and @see Overlay()), or OVERLAY_NONE.
	 * @returns    True on success, or false if an error occurred.
	 */
	bool Process( void* image, uint32_t width, uint32_t height, imageFormat format, std::vector<ObjectPose>& poses, uint32_t overlay=OVERLAY_DEFAULT );
	
	/**
	 * Perform pose estimation on the given image, and overlay the results.
	 * @param[in]  image input image in CUDA device memory (uchar3/uchar4/float3/float4)
	 * @param[in]  width width of the input image in pixels.
	 * @param[in]  height height of the input image in pixels.
	 * @param[in]  overlay bitwise OR combination of overlay flags (@see OverlayFlags and @see Overlay()), or OVERLAY_NONE.
	 * @returns    True on success, or false if an error occurred.
	 */
	template<typename T> bool Process( T* image, uint32_t width, uint32_t height, uint32_t overlay=OVERLAY_DEFAULT )											{ return Process((void*)image, width, height, imageFormatFromType<T>(), overlay); }

	/**
	 * Perform pose estimation on the given image, and overlay the results.
	 * @param[in]  image input image in CUDA device memory (uchar3/uchar4/float3/float4)
	 * @param[in]  width width of the input image in pixels.
	 * @param[in]  height height of the input image in pixels.
	 * @param[in]  overlay bitwise OR combination of overlay flags (@see OverlayFlags and @see Overlay()), or OVERLAY_NONE.
	 * @returns    True on success, or false if an error occurred.
	 */
	bool Process( void* image, uint32_t width, uint32_t height, imageFormat format, uint32_t overlay=OVERLAY_DEFAULT );

	/**
	 * Overlay the results on the image.
	 */
	template<typename T> bool Overlay( T* input, T* output, uint32_t width, uint32_t height, const std::vector<ObjectPose>& poses, uint32_t overlay=OVERLAY_DEFAULT )		{ return Overlay((void*)input, (void*)output, width, height, imageFormatFromType<T>(), overlay); }
	
	/**
	 * Overlay the results on the image.
	 */
	bool Overlay( void* input, void* output, uint32_t width, uint32_t height, imageFormat format, const std::vector<ObjectPose>& poses, uint32_t overlay=OVERLAY_DEFAULT );
	
	/**
	 * Retrieve the minimum confidence threshold.
	 */
	inline float GetThreshold() const							{ return mThreshold; }

	/**
	 * Set the minimum confidence threshold.
	 */
	inline void SetThreshold( float threshold ) 					{ mThreshold = threshold; }

	/**
	 * Get the category of objects that are detected (e.g. 'person', 'hand')
	 */
	inline const char* GetCategory() const						{ return mTopology.category.c_str(); }
	
	/**
	 * Get the number of keypoints in the topology.
	 */
	inline uint32_t GetNumKeypoints() const						{ return mTopology.keypoints.size(); }
	
	/**
	 * Get the name of a keypoint in the topology by it's ID.
	 */
	inline const char* GetKeypointName( uint32_t id ) const		{ return mTopology.keypoints[id].c_str(); }
	
	/**
	 * Find the ID of a keypoint by name, or return -1 if not found.
	 */
	inline int FindKeypointID( const char* name ) const;

	/**
 	 * Set overlay alpha blending value for all classes (between 0-255).
	 */
	//void SetOverlayAlpha( float alpha );
	
protected:

	static const int CMAP_WINDOW_SIZE=5;
	static const int PAF_INTEGRAL_SAMPLES=7;
	static const int MAX_LINKS=100;
	static const int MAX_OBJECTS=100;
	
	struct Topology
	{
		std::string category;
		std::vector<std::string> keypoints;
		int links[MAX_LINKS * 4];
		int numLinks;
	};
	
	// constructor
	poseNet();

	bool init( const char* model_path, const char* topology, float threshold, 
			 const char* input, const char* cmap, const char* paf, uint32_t maxBatchSize, 
			 precisionType precision, deviceType device, bool allowGPUFallback );

	bool postProcess(std::vector<ObjectPose>& poses, uint32_t width, uint32_t height);
	
	static bool loadTopology( const char* json_path, Topology* topology );
	
	Topology mTopology;
	float mThreshold;
	
	// post-processing buffers
	int* mPeaks;
	int* mPeakCounts;
	int* mConnections;
	int* mObjects;
	int  mNumObjects;
	
	float* mRefinedPeaks;
	float* mScoreGraph;
	
	void* mAssignmentWorkspace;
	void* mConnectionWorkspace;
};


// FindKeypointID
inline int poseNet::FindKeypointID( const char* name ) const
{
	if( !name )
		return -1;
	
	const uint32_t numKeypoints = GetNumKeypoints();
	
	for( uint32_t n=0; n < numKeypoints; n++ )
	{
		if( strcasecmp(GetKeypointName(n), name) == 0 )
			return n;
	}
	
	return -1;
}

// FindKeypoint
inline int poseNet::ObjectPose::FindKeypoint( uint32_t id ) const
{
	const uint32_t numKeypoints = Keypoints.size();
	
	for( uint32_t n=0; n < numKeypoints; n++ )
	{
		if( id == Keypoints[n].ID )
			return n;
	}
	
	return -1;
}

// FindLink
inline int poseNet::ObjectPose::FindLink( uint32_t a, uint32_t b ) const
{
	const uint32_t numLinks = Links.size();
	
	for( uint32_t n=0; n < numLinks; n++ )
	{
		if( a == Keypoints[Links[n][0]].ID && b == Keypoints[Links[n][1]].ID )
			return n;
	}
	
	return -1;
}


#endif
