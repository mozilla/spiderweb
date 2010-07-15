/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "FrameLayerBuilder.h"

#include "nsDisplayList.h"
#include "nsPresContext.h"
#include "nsLayoutUtils.h"
#include "Layers.h"

#ifdef DEBUG
#include <stdio.h>
#endif

using namespace mozilla::layers;

namespace mozilla {

namespace {

/**
 * This is the userdata we associate with a layer manager.
 */
class LayerManagerData {
public:
  LayerManagerData() :
    mInvalidateAllThebesContent(PR_FALSE),
    mInvalidateAllLayers(PR_FALSE)
  {
    mFramesWithLayers.Init();
  }

  /**
   * Tracks which frames have layers associated with them.
   */
  nsTHashtable<nsPtrHashKey<nsIFrame> > mFramesWithLayers;
  PRPackedBool mInvalidateAllThebesContent;
  PRPackedBool mInvalidateAllLayers;
};

static void DestroyRegion(void* aPropertyValue)
{
  delete static_cast<nsRegion*>(aPropertyValue);
}

/**
 * This property represents a region that should be invalidated in every
 * ThebesLayer child whose parent ContainerLayer is associated with the
 * frame. This is an nsRegion*; the coordinates of the region are
 * relative to the top-left of the border-box of the frame the property
 * is attached to (which is the frame for the ContainerLayer).
 * 
 * We add to this region in InvalidateThebesLayerContents. The region
 * is propagated to ContainerState in BuildContainerLayerFor, and then
 * the region(s) are actually invalidated in CreateOrRecycleThebesLayer.
 */
NS_DECLARE_FRAME_PROPERTY(ThebesLayerInvalidRegionProperty, DestroyRegion)

/**
 * This is a helper object used to build up the layer children for
 * a ContainerLayer.
 */
class ContainerState {
public:
  ContainerState(nsDisplayListBuilder* aBuilder,
                 LayerManager* aManager,
                 nsIFrame* aContainerFrame,
                 ContainerLayer* aContainerLayer) :
    mBuilder(aBuilder), mManager(aManager),
    mContainerFrame(aContainerFrame), mContainerLayer(aContainerLayer),
    mNextFreeRecycledThebesLayer(0),
    mInvalidateAllThebesContent(PR_FALSE)
  {
    CollectOldThebesLayers();
  }

  void SetInvalidThebesContent(const nsIntRegion& aRegion)
  {
    mInvalidThebesContent = aRegion;
  }
  void SetInvalidateAllThebesContent()
  {
    mInvalidateAllThebesContent = PR_TRUE;
  }
  /**
   * This is the method that actually walks a display list and builds
   * the child layers. We invoke it recursively to process clipped sublists.
   * @param aClipRect the clip rect to apply to the list items, or null
   * if no clipping is required
   */
  void ProcessDisplayItems(const nsDisplayList& aList,
                           const nsRect* aClipRect);
  /**
   * This finalizes all the open ThebesLayers by popping every element off
   * mThebesLayerDataStack, then sets the children of the container layer
   * to be all the layers in mNewChildLayers in that order and removes any
   * layers as children of the container that aren't in mNewChildLayers.
   */
  void Finish();

protected:
  /**
   * We keep a stack of these to represent the ThebesLayers that are
   * currently available to have display items added to.
   * We use a stack here because as much as possible we want to
   * assign display items to existing ThebesLayers, and to the lowest
   * ThebesLayer in z-order. This reduces the number of layers and
   * makes it more likely a display item will be rendered to an opaque
   * layer, giving us the best chance of getting subpixel AA.
   */
  class ThebesLayerData {
  public:
    ThebesLayerData() : mActiveScrolledRoot(nsnull), mLayer(nsnull) {}
    /**
     * Record that an item has been added to the ThebesLayer, so we
     * need to update our regions.
     */
    void Accumulate(const nsIntRect& aVisibleRect, PRBool aIsOpaque);
    nsIFrame* GetActiveScrolledRoot() { return mActiveScrolledRoot; }

    /**
     * The region of visible content in the layer, relative to the
     * container layer (which is at the snapped top-left of the display
     * list reference frame).
     */
    nsIntRegion  mVisibleRegion;
    /**
     * The region of visible content above the layer and below the
     * next ThebesLayerData currently in the stack, if any. Note that not
     * all ThebesLayers for the container are in the ThebesLayerData stack.
     * Same coordinate system as mVisibleRegion.
     */
    nsIntRegion  mVisibleAboveRegion;
    /**
     * The region of visible content in the layer that is opaque.
     * Same coordinate system as mVisibleRegion.
     */
    nsIntRegion  mOpaqueRegion;
    /**
     * The "active scrolled root" for all content in the layer. Must
     * be non-null; all content in a ThebesLayer must have the same
     * active scrolled root.
     */
    nsIFrame*    mActiveScrolledRoot;
    ThebesLayer* mLayer;
  };

  /**
   * Grab the next recyclable ThebesLayer, or create one if there are no
   * more recyclable ThebesLayers. Does any necessary invalidation of
   * a recycled ThebesLayer, and sets up the transform on the ThebesLayer
   * to account for scrolling. Adds the layer to mNewChildLayers.
   */
  already_AddRefed<ThebesLayer> CreateOrRecycleThebesLayer(nsIFrame* aActiveScrolledRoot);
  /**
   * Grabs all ThebesLayers from the ContainerLayer and makes them
   * available for recycling.
   */
  void CollectOldThebesLayers();
  /**
   * Indicate that we are done adding items to the ThebesLayer at the top of
   * mThebesLayerDataStack. Set the final visible region and opaque-content
   * flag, and pop it off the stack.
   */
  void PopThebesLayerData();
  /**
   * Find the ThebesLayer to which we should assign the next display item.
   * Returns the layer, and also updates the ThebesLayerData. Will
   * push a new ThebesLayerData onto the stack if necessary. If we choose
   * a ThebesLayer that's already on the ThebesLayerData stack,
   * later elements on the stack will be popped off.
   * @param aVisibleRect the area of the next display item that's visible
   * @param aActiveScrolledRoot the active scrolled root for the next
   * display item
   * @param aIsOpaque whether the bounds of the next display item are
   * opaque
   */
  already_AddRefed<ThebesLayer> FindThebesLayerFor(const nsIntRect& aVisibleRect,
                                                   nsIFrame* aActiveScrolledRoot,
                                                   PRBool aIsOpaque);
  ThebesLayerData* GetTopThebesLayerData()
  {
    return mThebesLayerDataStack.IsEmpty() ? nsnull
        : mThebesLayerDataStack[mThebesLayerDataStack.Length() - 1].get();
  }

  nsDisplayListBuilder*            mBuilder;
  LayerManager*                    mManager;
  nsIFrame*                        mContainerFrame;
  ContainerLayer*                  mContainerLayer;
  /**
   * The region of ThebesLayers that should be invalidated every time
   * we recycle one.
   */
  nsIntRegion                      mInvalidThebesContent;
  nsAutoTArray<nsAutoPtr<ThebesLayerData>,1>  mThebesLayerDataStack;
  /**
   * We collect the list of children in here. During ProcessDisplayItems,
   * the layers in this array either have mContainerLayer as their parent,
   * or no parent.
   */
  nsAutoTArray<nsRefPtr<Layer>,1>  mNewChildLayers;
  nsTArray<nsRefPtr<ThebesLayer> > mRecycledThebesLayers;
  PRUint32                         mNextFreeRecycledThebesLayer;
  PRPackedBool                     mInvalidateAllThebesContent;
};

/**
 * The address of gThebesDisplayItemLayerUserData is used as the user
 * data pointer for ThebesLayers created by FrameLayerBuilder.
 * It identifies ThebesLayers used to draw non-layer content, which are
 * therefore eligible for recycling. We want display items to be able to
 * create their own dedicated ThebesLayers in BuildLayer, if necessary,
 * and we wouldn't want to accidentally recycle those.
 */
static PRUint8 gThebesDisplayItemLayerUserData;

} // anonymous namespace

PRBool
FrameLayerBuilder::DisplayItemDataEntry::HasContainerLayer()
{
  for (PRUint32 i = 0; i < mData.Length(); ++i) {
    if (mData[i].mLayer->GetType() == Layer::TYPE_CONTAINER)
      return PR_TRUE;
  }
  return PR_FALSE;
}

/* static */ void
FrameLayerBuilder::InternalDestroyDisplayItemData(nsIFrame* aFrame,
                                                  void* aPropertyValue,
                                                  PRBool aRemoveFromFramesWithLayers)
{
  nsRefPtr<LayerManager> managerRef;
  nsTArray<DisplayItemData>* array =
    reinterpret_cast<nsTArray<DisplayItemData>*>(&aPropertyValue);
  NS_ASSERTION(!array->IsEmpty(), "Empty arrays should not be stored");

  if (aRemoveFromFramesWithLayers) {
    LayerManager* manager = array->ElementAt(0).mLayer->Manager();
    LayerManagerData* data = static_cast<LayerManagerData*>
      (manager->GetUserData());
    NS_ASSERTION(data, "Frame with layer should have been recorded");
    data->mFramesWithLayers.RemoveEntry(aFrame);
    if (data->mFramesWithLayers.Count() == 0) {
      delete data;
      manager->SetUserData(nsnull);
      // Consume the reference we added when we set the user data
      // in DidEndTransaction. But don't actually release until we've
      // released all the layers in the DisplayItemData array below!
      managerRef = manager;
      NS_RELEASE(manager);
    }
  }

  array->~nsTArray<DisplayItemData>();
}

/* static */ void
FrameLayerBuilder::DestroyDisplayItemData(nsIFrame* aFrame,
                                          void* aPropertyValue)
{
  InternalDestroyDisplayItemData(aFrame, aPropertyValue, PR_TRUE);
}

void
FrameLayerBuilder::BeginUpdatingRetainedLayers(LayerManager* aManager)
{
  mRetainingManager = aManager;
  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData());
  if (data) {
    mInvalidateAllThebesContent = data->mInvalidateAllThebesContent;
    mInvalidateAllLayers = data->mInvalidateAllLayers;
  }
}

/**
 * A helper function to remove the mThebesLayerItems entries for every
 * layer in aLayer's subtree.
 */
void
FrameLayerBuilder::RemoveThebesItemsForLayerSubtree(Layer* aLayer)
{
  ThebesLayer* thebes = aLayer->AsThebesLayer();
  if (thebes) {
    mThebesLayerItems.RemoveEntry(thebes);
    return;
  }

  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    RemoveThebesItemsForLayerSubtree(child);
  }
}

void
FrameLayerBuilder::DidEndTransaction(LayerManager* aManager)
{
  if (aManager != mRetainingManager) {
    Layer* root = aManager->GetRoot();
    if (root) {
      RemoveThebesItemsForLayerSubtree(root);
    }
    return;
  }

  // We need to save the data we'll need to support retaining.
  LayerManagerData* data = static_cast<LayerManagerData*>
    (mRetainingManager->GetUserData());
  if (data) {
    // Update all the frames that used to have layers.
    data->mFramesWithLayers.EnumerateEntries(UpdateDisplayItemDataForFrame, this);
  } else {
    data = new LayerManagerData();
    mRetainingManager->SetUserData(data);
    // Addref mRetainingManager. We'll release it when 'data' is
    // removed.
    NS_ADDREF(mRetainingManager);
  }
  // Now go through all the frames that didn't have any retained
  // display items before, and record those retained display items.
  // This also empties mNewDisplayItemData.
  mNewDisplayItemData.EnumerateEntries(StoreNewDisplayItemData, data);
  data->mInvalidateAllThebesContent = PR_FALSE;
  data->mInvalidateAllLayers = PR_FALSE;

  NS_ASSERTION(data->mFramesWithLayers.Count() > 0,
               "Some frame must have a layer!");
}

/* static */ PLDHashOperator
FrameLayerBuilder::UpdateDisplayItemDataForFrame(nsPtrHashKey<nsIFrame>* aEntry,
                                                 void* aUserArg)
{
  FrameLayerBuilder* builder = static_cast<FrameLayerBuilder*>(aUserArg);
  nsIFrame* f = aEntry->GetKey();
  FrameProperties props = f->Properties();
  DisplayItemDataEntry* newDisplayItems =
    builder->mNewDisplayItemData.GetEntry(f);
  if (!newDisplayItems) {
    // This frame was visible, but isn't anymore.
    PRBool found;
    void* prop = props.Remove(DisplayItemDataProperty(), &found);
    NS_ASSERTION(found, "How can the frame property be missing?");
    // Pass PR_FALSE to not remove from mFramesWithLayers, we'll remove it
    // by returning PL_DHASH_REMOVE below.
    // Note that DestroyDisplayItemData would delete the user data
    // for the retained layer manager if it removed the last entry from
    // mFramesWithLayers, but we won't. That's OK because our caller
    // is DidEndTransaction, which would recreate the user data
    // anyway.
    InternalDestroyDisplayItemData(f, prop, PR_FALSE);
    props.Delete(ThebesLayerInvalidRegionProperty());
    f->RemoveStateBits(NS_FRAME_HAS_CONTAINER_LAYER);
    return PL_DHASH_REMOVE;
  }

  if (!newDisplayItems->HasContainerLayer()) {
    props.Delete(ThebesLayerInvalidRegionProperty());
    f->RemoveStateBits(NS_FRAME_HAS_CONTAINER_LAYER);
  } else {
    NS_ASSERTION(f->GetStateBits() & NS_FRAME_HAS_CONTAINER_LAYER,
                 "This bit should have been set by BuildContainerLayerFor");
  }

  // Reset the invalid region now so we can start collecting new dirty
  // areas.
  nsRegion* invalidRegion = static_cast<nsRegion*>
    (props.Get(ThebesLayerInvalidRegionProperty()));
  if (invalidRegion) {
    invalidRegion->SetEmpty();
  }

  // We need to remove and re-add the DisplayItemDataProperty in
  // case the nsTArray changes the value of its mHdr.
  void* propValue = props.Remove(DisplayItemDataProperty());
  NS_ASSERTION(propValue, "mFramesWithLayers out of sync");
  PR_STATIC_ASSERT(sizeof(nsTArray<DisplayItemData>) == sizeof(void*));
  nsTArray<DisplayItemData>* array =
    reinterpret_cast<nsTArray<DisplayItemData>*>(&propValue);
  // Steal the list of display item layers
  array->SwapElements(newDisplayItems->mData);
  props.Set(DisplayItemDataProperty(), propValue);
  // Don't need to process this frame again
  builder->mNewDisplayItemData.RawRemoveEntry(newDisplayItems);
  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator
FrameLayerBuilder::StoreNewDisplayItemData(DisplayItemDataEntry* aEntry,
                                           void* aUserArg)
{
  LayerManagerData* data = static_cast<LayerManagerData*>(aUserArg);
  nsIFrame* f = aEntry->GetKey();
  // Remember that this frame has display items in retained layers
  NS_ASSERTION(!data->mFramesWithLayers.GetEntry(f),
               "We shouldn't get here if we're already in mFramesWithLayers");
  data->mFramesWithLayers.PutEntry(f);
  NS_ASSERTION(!f->Properties().Get(DisplayItemDataProperty()),
               "mFramesWithLayers out of sync");

  void* propValue;
  nsTArray<DisplayItemData>* array =
    new (&propValue) nsTArray<DisplayItemData>();
  // Steal the list of display item layers
  array->SwapElements(aEntry->mData);
  // Save it
  f->Properties().Set(DisplayItemDataProperty(), propValue);

  return PL_DHASH_REMOVE;
}

Layer*
FrameLayerBuilder::GetOldLayerFor(nsIFrame* aFrame, PRUint32 aDisplayItemKey)
{
  // If we need to build a new layer tree, then just refuse to recycle
  // anything.
  if (!mRetainingManager || mInvalidateAllLayers)
    return nsnull;

  void* propValue = aFrame->Properties().Get(DisplayItemDataProperty());
  if (!propValue)
    return nsnull;

  nsTArray<DisplayItemData>* array =
    (reinterpret_cast<nsTArray<DisplayItemData>*>(&propValue));
  for (PRUint32 i = 0; i < array->Length(); ++i) {
    if (array->ElementAt(i).mDisplayItemKey == aDisplayItemKey) {
      Layer* layer = array->ElementAt(i).mLayer;
      if (layer->Manager() == mRetainingManager)
        return layer;
    }
  }
  return nsnull;
}

/**
 * Invalidate aRegion in aLayer. aLayer is in the coordinate system
 * *after* aLayer's transform has been applied, so we need to
 * apply the inverse of that transform before calling InvalidateRegion.
 * Currently we assume that the transform is just an integer translation,
 * since that's all we need for scrolling.
 */
static void
InvalidatePostTransformRegion(ThebesLayer* aLayer, const nsIntRegion& aRegion)
{
  gfxMatrix transform;
  if (aLayer->GetTransform().Is2D(&transform)) {
    NS_ASSERTION(!transform.HasNonIntegerTranslation(),
                 "Matrix not just an integer translation?");
    // Convert the region from the coordinates of the container layer
    // (relative to the snapped top-left of the display list reference frame)
    // to the ThebesLayer's own coordinates
    nsIntRegion rgn = aRegion;
    rgn.MoveBy(-nsIntPoint(PRInt32(transform.x0), PRInt32(transform.y0)));
    aLayer->InvalidateRegion(rgn);
  } else {
    NS_ERROR("Only 2D transformations currently supported");
  }
}

already_AddRefed<ThebesLayer>
ContainerState::CreateOrRecycleThebesLayer(nsIFrame* aActiveScrolledRoot)
{
  // We need a new thebes layer
  nsRefPtr<ThebesLayer> layer;
  if (mNextFreeRecycledThebesLayer <
      mRecycledThebesLayers.Length()) {
    // Recycle a layer
    layer = mRecycledThebesLayers[mNextFreeRecycledThebesLayer];
    ++mNextFreeRecycledThebesLayer;

    // This gets called on recycled ThebesLayers that are going to be in the
    // final layer tree, so it's a convenient time to invalidate the
    // content that changed where we don't know what ThebesLayer it belonged
    // to, or if we need to invalidate the entire layer, we can do that.
    // This needs to be done before we update the ThebesLayer to its new
    // transform. See nsGfxScrollFrame::InvalidateInternal, where
    // we ensure that mInvalidThebesContent is updated according to the
    // scroll position as of the most recent paint.
    if (mInvalidateAllThebesContent) {
      nsIntRect invalidate = layer->GetValidRegion().GetBounds();
      layer->InvalidateRegion(invalidate);
    } else {
      InvalidatePostTransformRegion(layer, mInvalidThebesContent);
    }
    // We do not need to Invalidate these areas in the widget because we
    // assume the caller of InvalidateThebesLayerContents or
    // InvalidateAllThebesLayerContents has ensured
    // the area is invalidated in the widget.
  } else {
    // Create a new thebes layer
    layer = mManager->CreateThebesLayer();
    if (!layer)
      return nsnull;
    // Mark this layer as being used for Thebes-painting display items
    layer->SetUserData(&gThebesDisplayItemLayerUserData);
  }

  // Set up transform so that 0,0 in the Thebes layer corresponds to the
  // (pixel-snapped) top-left of the aActiveScrolledRoot.
  nsPoint offset = mBuilder->ToReferenceFrame(aActiveScrolledRoot);
  nsIntPoint pixOffset = offset.ToNearestPixels(
      aActiveScrolledRoot->PresContext()->AppUnitsPerDevPixel());
  gfxMatrix matrix;
  matrix.Translate(gfxPoint(pixOffset.x, pixOffset.y));
  layer->SetTransform(gfx3DMatrix::From2D(matrix));

  NS_ASSERTION(!mNewChildLayers.Contains(layer), "Layer already in list???");
  mNewChildLayers.AppendElement(layer);
  return layer.forget();
}

/**
 * Returns the appunits per dev pixel for the item's frame. The item must
 * have a frame because only nsDisplayClip items don't have a frame,
 * and those items are flattened away by ProcessDisplayItems.
 */
static PRUint32
AppUnitsPerDevPixel(nsDisplayItem* aItem)
{
  return aItem->GetUnderlyingFrame()->PresContext()->AppUnitsPerDevPixel();
}

/**
 * Set the visible rect of aLayer. aLayer is in the coordinate system
 * *after* aLayer's transform has been applied, so we need to
 * apply the inverse of that transform before calling SetVisibleRegion.
 */
static void
SetVisibleRectForLayer(Layer* aLayer, const nsIntRect& aRect)
{
  gfxMatrix transform;
  if (aLayer->GetTransform().Is2D(&transform)) {
    // if 'transform' is not invertible, then nothing will be displayed
    // for the layer, so it doesn't really matter what we do here
    transform.Invert();
    gfxRect layerVisible = transform.TransformBounds(
        gfxRect(aRect.x, aRect.y, aRect.width, aRect.height));
    layerVisible.RoundOut();
    nsIntRect visibleRect;
    if (NS_FAILED(nsLayoutUtils::GfxRectToIntRect(layerVisible, &visibleRect))) {
      NS_ERROR("Visible rect transformed out of bounds");
    }
    aLayer->SetVisibleRegion(visibleRect);
  } else {
    NS_ERROR("Only 2D transformations currently supported");
  }
}

void
ContainerState::PopThebesLayerData()
{
  NS_ASSERTION(!mThebesLayerDataStack.IsEmpty(), "Can't pop");

  PRInt32 lastIndex = mThebesLayerDataStack.Length() - 1;
  ThebesLayerData* data = mThebesLayerDataStack[lastIndex];

  if (lastIndex > 0) {
    // Since we're going to pop off the last ThebesLayerData, the
    // mVisibleAboveRegion of the second-to-last item will need to include
    // the regions of the last item.
    ThebesLayerData* nextData = mThebesLayerDataStack[lastIndex - 1];
    nextData->mVisibleAboveRegion.Or(nextData->mVisibleAboveRegion,
                                     data->mVisibleAboveRegion);
    nextData->mVisibleAboveRegion.Or(nextData->mVisibleAboveRegion,
                                     data->mVisibleRegion);
  }

  gfxMatrix transform;
  if (data->mLayer->GetTransform().Is2D(&transform)) {
    NS_ASSERTION(!transform.HasNonIntegerTranslation(),
                 "Matrix not just an integer translation?");
    // Convert from relative to the container to relative to the
    // ThebesLayer itself.
    nsIntRegion rgn = data->mVisibleRegion;
    rgn.MoveBy(-nsIntPoint(PRInt32(transform.x0), PRInt32(transform.y0)));
    data->mLayer->SetVisibleRegion(rgn);
  } else {
    NS_ERROR("Only 2D transformations currently supported");
  }

  nsIntRegion transparentRegion;
  transparentRegion.Sub(data->mVisibleRegion, data->mOpaqueRegion);
  data->mLayer->SetIsOpaqueContent(transparentRegion.IsEmpty());

  mThebesLayerDataStack.RemoveElementAt(lastIndex);
}

void
ContainerState::ThebesLayerData::Accumulate(const nsIntRect& aRect,
                                            PRBool aIsOpaque)
{
  mVisibleRegion.Or(mVisibleRegion, aRect);
  mVisibleRegion.SimplifyOutward(4);
  if (aIsOpaque) {
    mOpaqueRegion.Or(mOpaqueRegion, aRect);
    mOpaqueRegion.SimplifyInward(4);
  }
}

already_AddRefed<ThebesLayer>
ContainerState::FindThebesLayerFor(const nsIntRect& aVisibleRect,
                                   nsIFrame* aActiveScrolledRoot,
                                   PRBool aIsOpaque)
{
  PRInt32 i;
  PRInt32 lowestUsableLayerWithScrolledRoot = -1;
  PRInt32 topmostLayerWithScrolledRoot = -1;
  for (i = mThebesLayerDataStack.Length() - 1; i >= 0; --i) {
    ThebesLayerData* data = mThebesLayerDataStack[i];
    if (data->mVisibleAboveRegion.Intersects(aVisibleRect)) {
      ++i;
      break;
    }
    if (data->mActiveScrolledRoot == aActiveScrolledRoot) {
      lowestUsableLayerWithScrolledRoot = i;
      if (topmostLayerWithScrolledRoot < 0) {
        topmostLayerWithScrolledRoot = i;
      }
    }
    if (data->mVisibleRegion.Intersects(aVisibleRect))
      break;
  }
  if (topmostLayerWithScrolledRoot < 0) {
    --i;
    for (; i >= 0; --i) {
      ThebesLayerData* data = mThebesLayerDataStack[i];
      if (data->mActiveScrolledRoot == aActiveScrolledRoot) {
        topmostLayerWithScrolledRoot = i;
        break;
      }
    }
  }

  if (topmostLayerWithScrolledRoot >= 0) {
    while (PRUint32(topmostLayerWithScrolledRoot + 1) < mThebesLayerDataStack.Length()) {
      PopThebesLayerData();
    }
  }

  nsRefPtr<ThebesLayer> layer;
  ThebesLayerData* thebesLayerData = nsnull;
  if (lowestUsableLayerWithScrolledRoot < 0) {
    layer = CreateOrRecycleThebesLayer(aActiveScrolledRoot);
    thebesLayerData = new ThebesLayerData();
    mThebesLayerDataStack.AppendElement(thebesLayerData);
    thebesLayerData->mLayer = layer;
    thebesLayerData->mActiveScrolledRoot = aActiveScrolledRoot;
  } else {
    thebesLayerData = mThebesLayerDataStack[lowestUsableLayerWithScrolledRoot];
    layer = thebesLayerData->mLayer;
  }

  thebesLayerData->Accumulate(aVisibleRect, aIsOpaque);
  return layer.forget();
}

/*
 * Iterate through the non-clip items in aList and its descendants.
 * For each item we compute the effective clip rect. Each item is assigned
 * to a layer. We invalidate the areas in ThebesLayers where an item
 * has moved from one ThebesLayer to another. Also,
 * aState->mInvalidThebesContent is invalidated in every ThebesLayer.
 * We set the clip rect for items that generated their own layer.
 * (ThebesLayers don't need a clip rect on the layer, we clip the items
 * individually when we draw them.)
 * We set the visible rect for all layers, although the actual setting
 * of visible rects for some ThebesLayers is deferred until the calling
 * of ContainerState::Finish.
 */
void
ContainerState::ProcessDisplayItems(const nsDisplayList& aList,
                                    const nsRect* aClipRect)
{
  for (nsDisplayItem* item = aList.GetBottom(); item; item = item->GetAbove()) {
    if (item->GetType() == nsDisplayItem::TYPE_CLIP) {
      nsDisplayClip* clipItem = static_cast<nsDisplayClip*>(item);
      nsRect clip = clipItem->GetClipRect();
      if (aClipRect) {
        clip.IntersectRect(clip, *aClipRect);
      }
      ProcessDisplayItems(*clipItem->GetList(), &clip);
      continue;
    }

    PRInt32 appUnitsPerDevPixel = AppUnitsPerDevPixel(item);
    nsIntRect itemVisibleRect =
      item->GetVisibleRect().ToNearestPixels(appUnitsPerDevPixel);
    nsRefPtr<Layer> ownLayer = item->BuildLayer(mBuilder, mManager);
    // Assign the item to a layer
    if (ownLayer) {
      NS_ASSERTION(ownLayer->Manager() == mManager, "Wrong manager");
      NS_ASSERTION(ownLayer->GetUserData() != &gThebesDisplayItemLayerUserData,
                   "We shouldn't have a FrameLayerBuilder-managed layer here!");
      // It has its own layer. Update that layer's clip and visible rects.
      if (aClipRect) {
        ownLayer->IntersectClipRect(
            aClipRect->ToNearestPixels(appUnitsPerDevPixel));
      }
      ThebesLayerData* data = GetTopThebesLayerData();
      if (data) {
        data->mVisibleAboveRegion.Or(data->mVisibleAboveRegion, itemVisibleRect);
      }
      SetVisibleRectForLayer(ownLayer, itemVisibleRect);
      ContainerLayer* oldContainer = ownLayer->GetParent();
      if (oldContainer && oldContainer != mContainerLayer) {
        oldContainer->RemoveChild(ownLayer);
      }
      NS_ASSERTION(!mNewChildLayers.Contains(ownLayer),
                   "Layer already in list???");
      mNewChildLayers.AppendElement(ownLayer);
      mBuilder->LayerBuilder()->AddLayerDisplayItem(ownLayer, item);
    } else {
      nsIFrame* f = item->GetUnderlyingFrame();
      nsPoint offsetToActiveScrolledRoot;
      nsIFrame* activeScrolledRoot =
        nsLayoutUtils::GetActiveScrolledRootFor(f, mBuilder->ReferenceFrame(),
                                                &offsetToActiveScrolledRoot);
      NS_ASSERTION(offsetToActiveScrolledRoot == f->GetOffsetTo(activeScrolledRoot),
                   "Wrong offset");

      nsRefPtr<ThebesLayer> thebesLayer =
        FindThebesLayerFor(itemVisibleRect, activeScrolledRoot,
                           item->IsOpaque(mBuilder));
      
      NS_ASSERTION(f, "Display items that render using Thebes must have a frame");
      PRUint32 key = item->GetPerFrameKey();
      NS_ASSERTION(key, "Display items that render using Thebes must have a key");
      Layer* oldLayer = mBuilder->LayerBuilder()->GetOldLayerFor(f, key);
      if (oldLayer && thebesLayer != oldLayer) {
        NS_ASSERTION(oldLayer->AsThebesLayer(),
                     "The layer for a display item changed type!");
        // The item has changed layers.
        // Invalidate the bounds in the old layer and new layer.
        // The bounds might have changed, but we assume that any difference
        // in the bounds will have been invalidated for all Thebes layers
        // in the container via regular frame invalidation.
        nsRect bounds = item->GetBounds(mBuilder);
        nsIntRect r = bounds.ToOutsidePixels(appUnitsPerDevPixel);
        // Update the layer contents
        InvalidatePostTransformRegion(oldLayer->AsThebesLayer(), r);
        InvalidatePostTransformRegion(thebesLayer, r);

        // Ensure the relevant area of the window is repainted.
        // Note that the area we're currently repainting will not be
        // repainted again, thanks to the logic in nsFrame::InvalidateRoot.
        mContainerFrame->Invalidate(bounds - mBuilder->ToReferenceFrame(mContainerFrame));
      }

      mBuilder->LayerBuilder()->
        AddThebesDisplayItem(thebesLayer, item, aClipRect, mContainerFrame);
    }
  }
}

void
FrameLayerBuilder::AddThebesDisplayItem(ThebesLayer* aLayer,
                                        nsDisplayItem* aItem,
                                        const nsRect* aClipRect,
                                        nsIFrame* aContainerLayerFrame)
{
  AddLayerDisplayItem(aLayer, aItem);

  ThebesLayerItemsEntry* entry = mThebesLayerItems.PutEntry(aLayer);
  if (entry) {
    entry->mContainerLayerFrame = aContainerLayerFrame;
    NS_ASSERTION(aItem->GetUnderlyingFrame(), "Must have frame");
    entry->mItems.AppendElement(ClippedDisplayItem(aItem, aClipRect));
  }
}

void
FrameLayerBuilder::AddLayerDisplayItem(Layer* aLayer,
                                       nsDisplayItem* aItem)
{
  if (aLayer->Manager() != mRetainingManager)
    return;

  nsIFrame* f = aItem->GetUnderlyingFrame();
  DisplayItemDataEntry* entry = mNewDisplayItemData.PutEntry(f);
  if (entry) {
    entry->mData.AppendElement(DisplayItemData(aLayer, aItem->GetPerFrameKey()));
  }
}

void
ContainerState::CollectOldThebesLayers()
{
  for (Layer* layer = mContainerLayer->GetFirstChild(); layer;
       layer = layer->GetNextSibling()) {
    if (layer->GetUserData() != &gThebesDisplayItemLayerUserData) {
      // This layer is not for rendering Thebes-based display items
      continue;
    }
    ThebesLayer* thebes = layer->AsThebesLayer();
    NS_ASSERTION(thebes, "Wrong layer type");
    mRecycledThebesLayers.AppendElement(thebes);
  }
}

void
ContainerState::Finish()
{
  while (!mThebesLayerDataStack.IsEmpty()) {
    PopThebesLayerData();
  }

  for (PRUint32 i = 0; i <= mNewChildLayers.Length(); ++i) {
    // An invariant of this loop is that the layers in mNewChildLayers
    // with index < i are the first i child layers of mContainerLayer.
    Layer* layer;
    if (i < mNewChildLayers.Length()) {
      layer = mNewChildLayers[i];
      if (!layer->GetParent()) {
        // This is not currently a child of the container, so just add it
        // now.
        Layer* prevChild = i == 0 ? nsnull : mNewChildLayers[i - 1];
        mContainerLayer->InsertAfter(layer, prevChild);
        continue;
      }
      NS_ASSERTION(layer->GetParent() == mContainerLayer,
                   "Layer shouldn't be the child of some other container");
    } else {
      layer = nsnull;
    }

    // If layer is non-null, then it's already a child of the container,
    // so scan forward until we find it, removing the other layers we
    // don't want here.
    // If it's null, scan forward until we've removed all the leftover
    // children.
    Layer* nextOldChild = i == 0 ? mContainerLayer->GetFirstChild() :
      mNewChildLayers[i - 1]->GetNextSibling();
    while (nextOldChild != layer) {
      Layer* tmp = nextOldChild;
      nextOldChild = nextOldChild->GetNextSibling();
      mContainerLayer->RemoveChild(tmp);
    }
    // If non-null, 'layer' is now in the right place in the list, so we
    // can just move on to the next one.
  }
}

already_AddRefed<Layer>
FrameLayerBuilder::BuildContainerLayerFor(nsDisplayListBuilder* aBuilder,
                                          LayerManager* aManager,
                                          nsIFrame* aContainerFrame,
                                          nsDisplayItem* aContainerItem,
                                          const nsDisplayList& aChildren)
{
  FrameProperties props = aContainerFrame->Properties();
  PRUint32 containerDisplayItemKey =
    aContainerItem ? aContainerItem->GetPerFrameKey() : 0;
  NS_ASSERTION(aContainerFrame, "Container display items here should have a frame");
  NS_ASSERTION(!aContainerItem ||
               aContainerItem->GetUnderlyingFrame() == aContainerFrame,
               "Container display item must match given frame");

  nsRefPtr<ContainerLayer> containerLayer;
  if (aManager == mRetainingManager) {
    Layer* oldLayer = GetOldLayerFor(aContainerFrame, containerDisplayItemKey);
    if (oldLayer) {
      NS_ASSERTION(oldLayer->Manager() == aManager, "Wrong manager");
      NS_ASSERTION(oldLayer->GetType() == Layer::TYPE_CONTAINER,
                   "Wrong layer type");
      containerLayer = static_cast<ContainerLayer*>(oldLayer);
      // Clear clip rect, the caller will set it.
      containerLayer->SetClipRect(nsnull);
    }
  }
  if (!containerLayer) {
    // No suitable existing layer was found.
    containerLayer = aManager->CreateContainerLayer();
    if (!containerLayer)
      return nsnull;
  }

  ContainerState state(aBuilder, aManager, aContainerFrame, containerLayer);

  if (aManager == mRetainingManager) {
    DisplayItemDataEntry* entry = mNewDisplayItemData.PutEntry(aContainerFrame);
    if (entry) {
      entry->mData.AppendElement(
          DisplayItemData(containerLayer, containerDisplayItemKey));
    }

    if (mInvalidateAllThebesContent) {
      state.SetInvalidateAllThebesContent();
    }

    nsRegion* invalidThebesContent(static_cast<nsRegion*>
      (props.Get(ThebesLayerInvalidRegionProperty())));
    if (invalidThebesContent) {
      nsPoint offset = aBuilder->ToReferenceFrame(aContainerFrame);
      invalidThebesContent->MoveBy(offset);
      state.SetInvalidThebesContent(invalidThebesContent->
        ToOutsidePixels(aContainerFrame->PresContext()->AppUnitsPerDevPixel()));
      invalidThebesContent->MoveBy(-offset);
    } else {
      // Set up region to collect invalidation data
      props.Set(ThebesLayerInvalidRegionProperty(), new nsRegion());
    }
    aContainerFrame->AddStateBits(NS_FRAME_HAS_CONTAINER_LAYER);
  }

  state.ProcessDisplayItems(aChildren, nsnull);
  state.Finish();

  containerLayer->SetIsOpaqueContent(aChildren.IsOpaque());
  nsRefPtr<Layer> layer = containerLayer.forget();
  return layer.forget();
}

Layer*
FrameLayerBuilder::GetLeafLayerFor(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   nsDisplayItem* aItem)
{
  if (aManager != mRetainingManager)
    return nsnull;

  nsIFrame* f = aItem->GetUnderlyingFrame();
  NS_ASSERTION(f, "Can only call GetLeafLayerFor on items that have a frame");
  Layer* layer = GetOldLayerFor(f, aItem->GetPerFrameKey());
  if (!layer)
    return nsnull;
  if (layer->GetUserData() == &gThebesDisplayItemLayerUserData) {
    // This layer was created to render Thebes-rendered content for this
    // display item. The display item should not use it for its own
    // layer rendering.
    return nsnull;
  }
  // Clear clip rect; the caller is responsible for setting it.
  layer->SetClipRect(nsnull);
  return layer;
}

/* static */ void
FrameLayerBuilder::InvalidateThebesLayerContents(nsIFrame* aFrame,
                                                 const nsRect& aRect)
{
  nsRegion* invalidThebesContent = static_cast<nsRegion*>
    (aFrame->Properties().Get(ThebesLayerInvalidRegionProperty()));
  if (!invalidThebesContent)
    return;
  invalidThebesContent->Or(*invalidThebesContent, aRect);
  invalidThebesContent->SimplifyOutward(20);
}

/* static */ void
FrameLayerBuilder::InvalidateAllThebesLayerContents(LayerManager* aManager)
{
  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData());
  if (data) {
    data->mInvalidateAllThebesContent = PR_TRUE;
  }
}

/* static */ void
FrameLayerBuilder::InvalidateAllLayers(LayerManager* aManager)
{
  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData());
  if (data) {
    data->mInvalidateAllLayers = PR_TRUE;
  }
}

/* static */
PRBool
FrameLayerBuilder::HasDedicatedLayer(nsIFrame* aFrame, PRUint32 aDisplayItemKey)
{
  void* propValue = aFrame->Properties().Get(DisplayItemDataProperty());
  if (!propValue)
    return PR_FALSE;

  nsTArray<DisplayItemData>* array =
    (reinterpret_cast<nsTArray<DisplayItemData>*>(&propValue));
  for (PRUint32 i = 0; i < array->Length(); ++i) {
    if (array->ElementAt(i).mDisplayItemKey == aDisplayItemKey) {
      void* layerUserData = array->ElementAt(i).mLayer->GetUserData();
      if (layerUserData != &gColorLayerUserData &&
          layerUserData != &gThebesDisplayItemLayerUserData)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

/* static */ void
FrameLayerBuilder::DrawThebesLayer(ThebesLayer* aLayer,
                                   gfxContext* aContext,
                                   const nsIntRegion& aRegionToDraw,
                                   const nsIntRegion& aRegionToInvalidate,
                                   void* aCallbackData)
{
  nsDisplayListBuilder* builder = static_cast<nsDisplayListBuilder*>
    (aCallbackData);
  ThebesLayerItemsEntry* entry =
    builder->LayerBuilder()->mThebesLayerItems.GetEntry(aLayer);
  NS_ASSERTION(entry, "We shouldn't be drawing into a layer with no items!");

  gfxMatrix transform;
  if (!aLayer->GetTransform().Is2D(&transform)) {
    NS_ERROR("non-2D transform in our Thebes layer!");
    return;
  }
  NS_ASSERTION(!transform.HasNonIntegerTranslation(),
               "Matrix not just an integer translation?");
  // make the origin of the context coincide with the origin of the
  // ThebesLayer
  gfxContextMatrixAutoSaveRestore saveMatrix(aContext); 
  aContext->Translate(-gfxPoint(transform.x0, transform.y0));
  nsIntPoint offset(PRInt32(transform.x0), PRInt32(transform.y0));

  nsPresContext* presContext = entry->mContainerLayerFrame->PresContext();
  nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  nsRect r = (aRegionToInvalidate.GetBounds() + offset).
    ToAppUnits(appUnitsPerDevPixel);
  entry->mContainerLayerFrame->Invalidate(r);

  // Our list may contain content with different prescontexts at
  // different zoom levels. 'rc' contains the nsIRenderingContext
  // used for the previous display item, and lastPresContext is the
  // prescontext for that item. We also cache the clip state for that
  // item.
  // XXX maybe we should stop that from being true by forcing content with
  // different zoom levels into different layers?
  nsRefPtr<nsIRenderingContext> rc;
  nsPresContext* lastPresContext = nsnull;
  nsRect currentClip;
  PRBool setClipRect = PR_FALSE;

  PRUint32 i;
  // Update visible regions. We need perform visibility analysis again
  // because we may be asked to draw into part of a ThebesLayer that
  // isn't actually visible in the window (e.g., because a ThebesLayer
  // expanded its visible region to a rectangle internally), in which
  // case the mVisibleRect stored in the display item may be wrong.
  nsRegion visible = aRegionToDraw.ToAppUnits(appUnitsPerDevPixel);
  visible.MoveBy(NSIntPixelsToAppUnits(offset.x, appUnitsPerDevPixel),
                 NSIntPixelsToAppUnits(offset.y, appUnitsPerDevPixel));

  for (i = entry->mItems.Length(); i > 0; --i) {
    ClippedDisplayItem* cdi = &entry->mItems[i - 1];

    presContext = cdi->mItem->GetUnderlyingFrame()->PresContext();
    if (presContext->AppUnitsPerDevPixel() != appUnitsPerDevPixel) {
      // Some kind of zooming detected, just redraw the entire item
      nsRegion tmp(cdi->mItem->GetBounds(builder));
      cdi->mItem->RecomputeVisibility(builder, &tmp);
      continue;
    }

    if (!cdi->mHasClipRect || cdi->mClipRect.Contains(visible.GetBounds())) {
      cdi->mItem->RecomputeVisibility(builder, &visible);
      continue;
    }

    // Do a little dance to account for the fact that we're clipping
    // to cdi->mClipRect
    nsRegion clipped;
    clipped.And(visible, cdi->mClipRect);
    nsRegion finalClipped = clipped;
    cdi->mItem->RecomputeVisibility(builder, &finalClipped);
    nsRegion removed;
    removed.Sub(clipped, finalClipped);
    nsRegion newVisible;
    newVisible.Sub(visible, removed);
    // Don't let the visible region get too complex.
    if (newVisible.GetNumRects() <= 15) {
      visible = newVisible;
    }
  }

  for (i = 0; i < entry->mItems.Length(); ++i) {
    ClippedDisplayItem* cdi = &entry->mItems[i];

    if (cdi->mItem->GetVisibleRect().IsEmpty())
      continue;

    presContext = cdi->mItem->GetUnderlyingFrame()->PresContext();
    // If the new desired clip state is different from the current state,
    // update the clip.
    if (setClipRect != cdi->mHasClipRect ||
        (cdi->mHasClipRect && cdi->mClipRect != currentClip)) {
      if (setClipRect) {
        aContext->Restore();
      }
      setClipRect = cdi->mHasClipRect;
      if (setClipRect) {
        currentClip = cdi->mClipRect;
        aContext->Save();
        aContext->NewPath();
        gfxRect clip(currentClip.x, currentClip.y, currentClip.width, currentClip.height);
        clip.ScaleInverse(presContext->AppUnitsPerDevPixel());
        aContext->Rectangle(clip, PR_TRUE);
        aContext->Clip();
      }
    }

    if (presContext != lastPresContext) {
      // Create a new rendering context with the right
      // appunits-per-dev-pixel.
      nsresult rv =
        presContext->DeviceContext()->CreateRenderingContextInstance(*getter_AddRefs(rc));
      if (NS_FAILED(rv))
        break;
      rc->Init(presContext->DeviceContext(), aContext);
      lastPresContext = presContext;
    }
    cdi->mItem->Paint(builder, rc);
  }

  if (setClipRect) {
    aContext->Restore();
  }
}

#ifdef DEBUG
static void
DumpIntRegion(FILE* aStream, const char* aName, const nsIntRegion& aRegion)
{
  if (aRegion.IsEmpty())
    return;

  fprintf(aStream, " [%s=", aName);
  nsIntRegionRectIterator iter(aRegion);
  const nsIntRect* r;
  PRBool first = PR_TRUE;
  while ((r = iter.Next()) != nsnull) {
    if (!first) {
      fputs(";", aStream);
    } else {
      first = PR_FALSE;
    }
    fprintf(aStream, "%d,%d,%d,%d", r->x, r->y, r->width, r->height);
  }
  fputs("]", aStream);
}

static void
DumpLayer(FILE* aStream, Layer* aLayer, PRUint32 aIndent)
{
  if (!aLayer)
    return;

  for (PRUint32 i = 0; i < aIndent; ++i) {
    fputs("  ", aStream);
  }
  const char* name = aLayer->Name();
  ThebesLayer* thebes = aLayer->AsThebesLayer();
  fprintf(aStream, "%s(%p)", name, aLayer);

  DumpIntRegion(aStream, "visible", aLayer->GetVisibleRegion());

  gfx3DMatrix transform = aLayer->GetTransform();
  if (!transform.IsIdentity()) {
    gfxMatrix matrix;
    if (transform.Is2D(&matrix)) {
      fprintf(aStream, " [transform=%g,%g; %g,%g; %g,%g]",
              matrix.xx, matrix.yx, matrix.xy, matrix.yy, matrix.x0, matrix.y0);
    } else {
      fprintf(aStream, " [transform=%g,%g,%g,%g; %g,%g,%g,%g; %g,%g,%g,%g; %g,%g,%g,%g]",
              transform._11, transform._12, transform._13, transform._14,
              transform._21, transform._22, transform._23, transform._24,
              transform._31, transform._32, transform._33, transform._34,
              transform._41, transform._42, transform._43, transform._44);
    }
  }

  const nsIntRect* clip = aLayer->GetClipRect();
  if (clip) {
    fprintf(aStream, " [clip=%d,%d,%d,%d]",
            clip->x, clip->y, clip->width, clip->height);
  }

  float opacity = aLayer->GetOpacity();
  if (opacity != 1.0) {
    fprintf(aStream, " [opacity=%f]", opacity);
  }

  if (aLayer->IsOpaqueContent()) {
    fputs(" [opaqueContent]", aStream);
  }

  if (thebes) {
    DumpIntRegion(aStream, "valid", thebes->GetValidRegion());
  }

  fputs("\n", aStream);

  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    DumpLayer(aStream, child, aIndent + 1);
  }
}

/* static */ void
FrameLayerBuilder::DumpLayerTree(LayerManager* aManager)
{
  DumpLayer(stderr, aManager->GetRoot(), 0);
}

void
FrameLayerBuilder::DumpRetainedLayerTree()
{
  if (mRetainingManager) {
    DumpLayerTree(mRetainingManager);
  }
}
#endif

} // namespace mozilla
