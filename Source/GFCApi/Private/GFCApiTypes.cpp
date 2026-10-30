#include "GFCApiTypes.h"

#include "Misc/Paths.h"

EGFCMediaKind FGFCSeed::GetMediaKind() const
{
	// The node reports "image", "video", "audio", "3d" or "other" (app_seeds.CONTENT_TYPE_MAP).
	// Older nodes or peers may send MIME types, so accept those too.
	const FString C = ContentType.ToLower();
	if (C == TEXT("3d") || C.StartsWith(TEXT("model/")))
	{
		return EGFCMediaKind::Model3D;
	}
	if (C == TEXT("image") || C.StartsWith(TEXT("image/")))
	{
		return EGFCMediaKind::Image;
	}
	if (C == TEXT("video") || C.StartsWith(TEXT("video/")))
	{
		return EGFCMediaKind::Video;
	}
	if (C == TEXT("audio") || C.StartsWith(TEXT("audio/")))
	{
		return EGFCMediaKind::Audio;
	}
	if (C == TEXT("text") || C.StartsWith(TEXT("text/")))
	{
		return EGFCMediaKind::Text;
	}

	const FString Ext = FPaths::GetExtension(Full, false).ToLower();
	static const TSet<FString> Model{TEXT("glb"), TEXT("gltf"), TEXT("fbx"), TEXT("obj"), TEXT("usdz"), TEXT("stl"), TEXT("3ds")};
	static const TSet<FString> Image{TEXT("jpg"), TEXT("jpeg"), TEXT("png"), TEXT("gif"), TEXT("webp"), TEXT("bmp"), TEXT("tiff"), TEXT("tga")};
	static const TSet<FString> Video{TEXT("mp4"), TEXT("mov"), TEXT("avi"), TEXT("webm"), TEXT("mkv"), TEXT("wmv"), TEXT("flv")};
	static const TSet<FString> Audio{TEXT("mp3"), TEXT("wav"), TEXT("ogg"), TEXT("oga"), TEXT("opus"), TEXT("flac"), TEXT("aac"),
		TEXT("m4a"), TEXT("aif"), TEXT("aiff"), TEXT("wma")};
	if (Model.Contains(Ext)) return EGFCMediaKind::Model3D;
	if (Image.Contains(Ext)) return EGFCMediaKind::Image;
	if (Video.Contains(Ext)) return EGFCMediaKind::Video;
	if (Audio.Contains(Ext)) return EGFCMediaKind::Audio;
	if (Ext == TEXT("txt") || Ext == TEXT("md")) return EGFCMediaKind::Text;
	// An exclusive text post has no public file at all (an exclusive video still says "video").
	if (bExclusive && Full.IsEmpty() && Teaser.IsEmpty()) return EGFCMediaKind::Text;
	return EGFCMediaKind::Other;
}
