man_MANS = meteorBuild.3 meteorMerge.3 meteorReset.3 meteorTriangleCount.3 \
meteorClip.3 meteorMultMatrix.3 meteorRewind.3 meteorTriangleCreatedCount.3 \
meteorColorFunc.3 meteorNormalFunc.3 meteorRotate.3 meteorTriangleMergeableCount.3 \
meteorCorrectTexCoords.3 meteorPointCount.3 meteorSave.3 meteorWritePoints.3 \
meteorError.3 meteorPointCreatedCount.3 meteorScale.3 meteorWriteTriangles.3 \
meteorFormat.3 meteorFreeMem.3 meteorPropagate.3 meteorSetSize.3 \
meteorFunc.3 meteorReadPoints.3 meteorTexCoordFunc.3 \
meteorLoad.3 meteorReadTriangles.3 meteorTranslate.3 meteor.1

EXTRA_DIST = *.3 *.1

HTMLMANS = $(patsubst %,%.html,$(man_MANS))

man2html:
	mkdir -p html
	$(MAKE) man2html-int

man2html-int: $(HTMLMANS)

%.html: %
	man2html -r $* > html/$@
