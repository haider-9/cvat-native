add_test([=[Core.VersionMacroIsAvailable]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Core.VersionMacroIsAvailable]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Core.VersionMacroIsAvailable]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_main.cpp:13]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Result.HoldsValue]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Result.HoldsValue]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Result.HoldsValue]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_result.cpp:7]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Result.HoldsError]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Result.HoldsError]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Result.HoldsError]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_result.cpp:15]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Result.RetryableClassification]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Result.RetryableClassification]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Result.RetryableClassification]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_result.cpp:24]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ApiClient.ResolveKeepsBasePath]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=ApiClient.ResolveKeepsBasePath]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ApiClient.ResolveKeepsBasePath]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_api_client.cpp:115]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ApiClient.GetParsesJsonAndAuthorization]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=ApiClient.GetParsesJsonAndAuthorization]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ApiClient.GetParsesJsonAndAuthorization]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_api_client.cpp:125]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ApiClient.HttpErrorBecomesUnauthorized]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=ApiClient.HttpErrorBecomesUnauthorized]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ApiClient.HttpErrorBecomesUnauthorized]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_api_client.cpp:149]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ApiClient.ErrorFromResponseMarksAuth]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=ApiClient.ErrorFromResponseMarksAuth]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ApiClient.ErrorFromResponseMarksAuth]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_api_client.cpp:166]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Annotation.ParsesLabeledData]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Annotation.ParsesLabeledData]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Annotation.ParsesLabeledData]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_annotation.cpp:49]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Annotation.SerializationRoundTrip]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Annotation.SerializationRoundTrip]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Annotation.SerializationRoundTrip]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_annotation.cpp:63]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Annotation.TrackInterpolates]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Annotation.TrackInterpolates]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Annotation.TrackInterpolates]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_annotation.cpp:77]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Annotation.ShapeTypes]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Annotation.ShapeTypes]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Annotation.ShapeTypes]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_annotation.cpp:98]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Annotation.CopiedTemplateDimensionAndPlacement]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Annotation.CopiedTemplateDimensionAndPlacement]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Annotation.CopiedTemplateDimensionAndPlacement]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_annotation.cpp:135]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[AnnotationModel.AddingAndUpdatingShapesIncrementsRevisionAndDirty]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=AnnotationModel.AddingAndUpdatingShapesIncrementsRevisionAndDirty]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AnnotationModel.AddingAndUpdatingShapesIncrementsRevisionAndDirty]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_annotation.cpp:106]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Geometry.BoundingRect]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Geometry.BoundingRect]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Geometry.BoundingRect]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_geometry.cpp:8]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Geometry.RectangleUsesOppositeCorners]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Geometry.RectangleUsesOppositeCorners]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Geometry.RectangleUsesOppositeCorners]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_geometry.cpp:17]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Geometry.PointInPolygon]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Geometry.PointInPolygon]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Geometry.PointInPolygon]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_geometry.cpp:26]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Geometry.DistanceToSegment]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Geometry.DistanceToSegment]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Geometry.DistanceToSegment]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_geometry.cpp:34]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Geometry.HitTestRectangleAndPolygon]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Geometry.HitTestRectangleAndPolygon]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Geometry.HitTestRectangleAndPolygon]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_geometry.cpp:40]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Geometry.TranslateAndScale]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Geometry.TranslateAndScale]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Geometry.TranslateAndScale]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_geometry.cpp:56]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Geometry.RotatedRectangle]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=Geometry.RotatedRectangle]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Geometry.RotatedRectangle]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_geometry.cpp:65]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[LocalStore.InMemoryRoundTrip]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=LocalStore.InMemoryRoundTrip]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[LocalStore.InMemoryRoundTrip]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_local_store.cpp:19]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[LocalStore.SnapshotAndPendingEdits]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=LocalStore.SnapshotAndPendingEdits]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[LocalStore.SnapshotAndPendingEdits]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_local_store.cpp:36]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[LocalStore.PrunesOldPendingEdits]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=LocalStore.PrunesOldPendingEdits]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[LocalStore.PrunesOldPendingEdits]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_local_store.cpp:64]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[RecoveryStore.WrapsSnapshots]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=RecoveryStore.WrapsSnapshots]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[RecoveryStore.WrapsSnapshots]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_local_store.cpp:75]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FrameCache.MemoryLruEviction]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=FrameCache.MemoryLruEviction]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FrameCache.MemoryLruEviction]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_frame_cache.cpp:23]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FrameCache.DiskRoundTripAndJobRemoval]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=FrameCache.DiskRoundTripAndJobRemoval]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FrameCache.DiskRoundTripAndJobRemoval]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_frame_cache.cpp:47]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FrameCache.DiskPruningUsesLeastRecentlyAccessed]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=FrameCache.DiskPruningUsesLeastRecentlyAccessed]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FrameCache.DiskPruningUsesLeastRecentlyAccessed]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_frame_cache.cpp:66]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FrameCache.CorruptDiskFrameIsRemovedAfterDecodeFailure]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=FrameCache.CorruptDiskFrameIsRemovedAfterDecodeFailure]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FrameCache.CorruptDiskFrameIsRemovedAfterDecodeFailure]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_frame_cache.cpp:90]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FrameCache.SeparatesQualityNamespaces]=]  /home/haider/Projects/cvat-native/build/cvat_core_tests [==[--gtest_filter=FrameCache.SeparatesQualityNamespaces]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FrameCache.SeparatesQualityNamespaces]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/haider/Projects/cvat-native/tests/test_frame_cache.cpp:117]==]
    WORKING_DIRECTORY [==[/home/haider/Projects/cvat-native/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(cvat_core_tests_TESTS [==[Core.VersionMacroIsAvailable]==] [==[Result.HoldsValue]==] [==[Result.HoldsError]==] [==[Result.RetryableClassification]==] [==[ApiClient.ResolveKeepsBasePath]==] [==[ApiClient.GetParsesJsonAndAuthorization]==] [==[ApiClient.HttpErrorBecomesUnauthorized]==] [==[ApiClient.ErrorFromResponseMarksAuth]==] [==[Annotation.ParsesLabeledData]==] [==[Annotation.SerializationRoundTrip]==] [==[Annotation.TrackInterpolates]==] [==[Annotation.ShapeTypes]==] [==[Annotation.CopiedTemplateDimensionAndPlacement]==] [==[AnnotationModel.AddingAndUpdatingShapesIncrementsRevisionAndDirty]==] [==[Geometry.BoundingRect]==] [==[Geometry.RectangleUsesOppositeCorners]==] [==[Geometry.PointInPolygon]==] [==[Geometry.DistanceToSegment]==] [==[Geometry.HitTestRectangleAndPolygon]==] [==[Geometry.TranslateAndScale]==] [==[Geometry.RotatedRectangle]==] [==[LocalStore.InMemoryRoundTrip]==] [==[LocalStore.SnapshotAndPendingEdits]==] [==[LocalStore.PrunesOldPendingEdits]==] [==[RecoveryStore.WrapsSnapshots]==] [==[FrameCache.MemoryLruEviction]==] [==[FrameCache.DiskRoundTripAndJobRemoval]==] [==[FrameCache.DiskPruningUsesLeastRecentlyAccessed]==] [==[FrameCache.CorruptDiskFrameIsRemovedAfterDecodeFailure]==] [==[FrameCache.SeparatesQualityNamespaces]==])
