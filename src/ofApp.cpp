#include "ofApp.h"


//--------------------------------------------------------------
void ofApp::setup(){
    // general
    // ofSetBackgroundAuto(false);
    ofSetVerticalSync(true);
    ofSetFrameRate(15);
    varSetup();
    ofSetBackgroundColor(0);
    ofSetWindowShape(1000, 1000);
//    ofSetWindowShape(800, 400);
//    ofSetWindowPosition(500, 500);
    // text loading
    ofBuffer buffer = ofBufferFromFile("txt/love_lyrics.txt");
    //
    if(buffer.size()) {
        for (ofBuffer::Line it = buffer.getLines().begin(), end = buffer.getLines().end(); it != end; ++it) {
            string line = *it;
            if(line.empty() == false) {
                textFileLines.push_back(line);
            }
        }
    }
    // GUI
    gui.setup();
    // TTS
    log.start();
    // ft
    ft.setup("../../../../models/shape_predictor_68_face_landmarks.dat");
    ft.setFaceDetectorImageSize(2000000);
    ft.setLandmarkDetectorImageSize(1000000);
    ft.setSmoothingRate(1);
    ft.setThreaded(true);
    // grid
    grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare);
    // video recording
    vidRecorder.setVideoCodec("mpeg4");
    vidRecorder.setVideoBitrate("2000k");
//    vidRecorder.setAudioCodec("mp3");
//    vidRecorder.setAudioBitrate("320k");
    ofAddListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::recordingComplete);
    //
    loadVideos();
    // capture
    #ifdef _USE_LIVE_VIDEO
        #ifdef _USE_BLACKMAGIC
            blackCam.setup(1920, 1080, 30);
        #else
            cam.setDeviceID(0);
            cam.setup(1920, 1080);
        #endif
    #else
        movie.load("vids/Busy business commuters in London.mp4");  // 1280x720
        movie.play();
    #endif
    // Live
    live.setup();
    initLive();
}


//--------------------------------------------------------------
void ofApp::update(){
    //
    live.update();
    refreshLive();
    //
    bool newFrame = false;
    ofSetWindowTitle(ofToString(ofGetFrameRate()));
    //
    #ifdef _USE_LIVE_VIDEO
        #ifdef _USE_BLACKMAGIC
            newFrame = blackCam.update();
        #else
            cam.update();
            newFrame = cam.isFrameNew();
        #endif
    #else
        movie.update();
        newFrame = movie.isFrameNew();
    #endif
    
    if(newFrame){
        // capture
        #ifdef _USE_LIVE_VIDEO
            #ifdef _USE_BLACKMAGIC
                srcImg.setFromPixels(blackCam.getColorPixels());
            #else
                srcImg.setFromPixels(cam.getPixels());
            #endif
        #else
            srcImg.setFromPixels(movie.getPixels());
        #endif
        
        // mirror and crop the source
        srcImg.mirror(false, true);
        srcImg.crop((srcImg.getWidth()-srcImg.getHeight())/2, 0, srcImg.getHeight(), srcImg.getHeight());
        // Filter  image
        if (srcImgIsFiltered) {
            clahe.filter(srcImg, srcImg, claheClipLimit, srcImgIsColored);
            srcImg.update();
        }
        ft.update(srcImg.getPixels());
        srcPixels = srcImg.getPixels();
        
        // *********
        // Faces aren't found
        if ( ft.size() == 0 ) {
            // Faces were found before but are not detected anymore
            if (facesFound) {
                // start iddle timer
                timer01.reset();
                timer01.startTimer();
                // reset bools
                facesFound = false, playVideos = true, showGrid = false, isFocused = false;
                // stop vid recorder
                vidRecorder.close();
                isRecording = false;
                cout << "close recording " << endl;
                // reset avg face w / h
                faceAvgWidth = 0, faceAvgHeight = 0, faceTotalFrame = 0, faceTotalWidth = 0, faceTotalHeight = 0;
                //
                focusTime = 20;
            }
            // *********
            // Idle mode
            // If the first timer is finished we enter idle mode
            if (timer01.isTimerFinished()) {
                // set all variables
                if (!isIdle) {
                    isIdle = true;
                    focusTime = 20 + (timeOut02/100);
                    // change the volume of track2
                    initTimesVolumes[0] = ofGetElapsedTimef(), startVolumes[0] = .4, endVolumes[0] = .2;
                    initTimesVolumes[1] = ofGetElapsedTimef(), startVolumes[1] = .6, endVolumes[1] = .2;
                    initTimesVolumes[2] = ofGetElapsedTimef(), startVolumes[2] = .4, endVolumes[2] = .4;
                    initTimesVolumes[3] = ofGetElapsedTimef(), startVolumes[3] = .4, endVolumes[3] = .4;
                    initTimesVolumes[4] = ofGetElapsedTimef(), startVolumes[4] = .6, endVolumes[4] = .4;
                    loadVideos();
                }
            }
        // *********
        // Else some faces are detected
        } else {
            // It comes from idle mode
            if (isIdle && !facesFound) {
                facesFound = true;
                // start 2nd timer
                timer02.reset();
                timer02.startTimer();
            } else if (!facesFound) {
                facesFound = true;
                // stop vid recorder
                vidRecorder.close();
                isRecording = false;
                cout << "close recording " << endl;
            }
            // *********
            // If the 2nd timer is finished show the capture
            if (timer02.isTimerFinished()) {
                // Stop the idle mode if necessary
                if (isIdle) {
                    stopVideos();
                    isIdle = false;
                    // Change the volume of track 1
                    initTimesVolumes[0] = ofGetElapsedTimef(), startVolumes[0] = .1, endVolumes[0] = .4;
                    initTimesVolumes[1] = ofGetElapsedTimef(), startVolumes[1] = .1, endVolumes[1] = .6;
                    initTimesVolumes[2] = ofGetElapsedTimef(), startVolumes[2] = .2, endVolumes[2] = .4;
                    initTimesVolumes[3] = ofGetElapsedTimef(), startVolumes[3] = .2, endVolumes[3] = .4;
                    initTimesVolumes[4] = ofGetElapsedTimef(), startVolumes[4] = .2, endVolumes[4] = .6;
                }
                // get faces
                vector<ofGrid::PixelsItem> pis;
                int i = 0;
//                bool focusedFaceExists = false;
                for (auto & face : ft.getInstances()) {
                    // *********
                    // If one face is present more than XX seconds
                    // Save face label and set isFocused to true
                    if (face.getAge() > focusTime && !isFocused) {
                        //
                        focusedFaceLabel = face.getLabel();
                        isFocused = true;
                        // start the timer
                        timer03.reset();
                        timer03.startTimer();
                        focusTime = 20;
                    }
                    // if the face is the one focused on
                    if (isFocused && face.getLabel()==focusedFaceLabel){
                        // if not yet recording start the recording
                        if (!isRecording) {
                            vidRecorder.setup( faceVideoPath + "/" + ofGetTimestampString("%Y%m%d-%H%M%S") + "-" + ofToString(focusedFaceLabel) + ".mov", 256, 256, (int)ofGetFrameRate() ); // no audio
//                            vidRecorder.setup( faceVideoPath + "/test.mov", 256, 256, (int)ofGetFrameRate() ); // no audio
                            vidRecorder.start();
                            isRecording = true;
                        }
                        if(isRecording && vidRecorder.isInitialized()){
                            // create a rectangle with the average width and height of the face
                            faceTotalFrame ++;
                            faceTotalWidth += face.getBoundingBox().getWidth();
                            faceTotalHeight += face.getBoundingBox().getHeight();
                            faceAvgWidth = faceTotalWidth / faceTotalFrame;
                            faceAvgHeight = faceTotalHeight / faceTotalFrame;
                            ofRectangle r;
                            r.setFromCenter(face.getBoundingBox().getCenter(), faceAvgWidth, faceAvgHeight);
                            focusedFaceRect = r;
                            // extract the face and record it
//                            ofImage img = srcPixels;
//                            img.resize(256, 256);
//                            vidRecorder.addFrame(img);
                            ofPixels faceCropped = getFacePart(srcPixels, ofPolyline::fromRectangle(r), 1, .5, 0, true);
                            if (faceCropped.resize(vidRecorder.getWidth(),vidRecorder.getHeight())) {
                                bool success = vidRecorder.addFrame(faceCropped);
                                if (!success) ofLogWarning("This frame was not added!");
                            }
                        }
                        
                        // after a certain time, show the grid
                        if (timer03.isTimerFinished()) {
                            if (!showGrid) {
                                randomizeSettings();
                                grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare);
                                showGrid = true;
                                initTimeGrid = ofGetElapsedTimef();
                            }
                        }
//                        focusedFaceExists = true;
                    }
                    
                    // select face elements for the grid
                    for (int i=0; i<20; i++) {
                        vector<ofPolyline> facePolylines {
                            ofPolyline::fromRectangle(face.getBoundingBox()),
                            face.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::LEFT_EYE),
                            face.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::RIGHT_EYE),
                            face.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::NOSE_BASE),
                            face.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::OUTER_MOUTH)
                        };
                        int j = 0;
                        for (auto & facePolyline : facePolylines) {
                            if (i < faceElementsQty.at(j)) {
                                pis.push_back(ofGrid::PixelsItem(getFacePart(srcPixels, facePolyline, 1, faceElementsZoom.at(j), i*faceElementsOffset.at(j), true), ofGrid::rightEye));
                            }
                            j++;
                        }
                    }
                }
//                isFocused = focusedFaceExists;
                
                // Grid
                if (showGrid) {
                    // update
                    grid.updatePixels(pis);
                    // easing of alpha
                    int s = grid.GridElements.size();
                    if (s) {
                        for (int i=0; i<grid.GridElements.size(); i++) {
                            float t = 5.f;
                            float d = 2.f;
                            auto startTime = initTimeGrid+(float)i/s*t;
                            auto endTime = initTimeGrid+(float)i/s*t + d;
                            auto now = ofGetElapsedTimef();
                            grid.GridElements.at(i).setAlpha( ofxeasing::map_clamp(now, startTime, endTime, 0, 255, &ofxeasing::linear::easeOut) );
                        }
                    }
                }
            }
        }
        
        //
        if (isIdle) updateVideos();
        
        // text
        if ( showGrid || isIdle ) {
            if (!log.startSpeaking && timer04.isTimerFinished()) {
                showText = true;
                timer04.reset();
                timer05.reset();
                timer05.stopTimer();
                // build speech settings
                string voice = "Kate";
                string msg = textFileLines.at(textFileIndex);
                string misc = "";
                log.LogAudio(voice, "", "", "130", "1", msg);
                // increment counters
                textFileIndex = (textFileIndex+1)%textFileLines.size();
                textContentIndex = (textContentIndex+1)%textContent.size();
                // clear current text
                textContent.at(textContentIndex).clear();
            } else if (textContentIndex == 0){
                // last sentence
                // start long speech timer
                timer04.setTimer(ofRandom(timeOut04, timeOut04*2));
                timer04.startTimer();
                timer05.startTimer();
            } else {
                // start speech timer
                timer04.setTimer(4000+ofRandom(2000));
                timer04.startTimer();
            }
            if (log.speechUpdate()) {
                // it's speaking, update words
                textContent.at(textContentIndex).append(log.getCurrentWord() + " ");
            }
            if (timer05.isTimerFinished()) {
                showText = false;
                textContent.resize(3);
                timer05.reset();
                timer05.stopTimer();
            }
        }
    }
}


//--------------------------------------------------------------
void ofApp::draw(){
    
    ofPushMatrix();
        ofScale(sceneScale, sceneScale);
        // We're not focused on one face
        if (!isFocused) {
            // draw outputImg
            srcImg.draw(0, 0, 192, 192);
            // draw facetracker
            ofPushMatrix();
                ofScale(192/srcImg.getWidth(), 192/srcImg.getWidth());
                ft.drawDebug();
            ofPopMatrix();
        // Else we are focused on one face
        } else {
            int focusSize = faceAvgWidth*1.2;
            int x = focusedFaceRect.getCenter().x - focusSize/2;
            int y = focusedFaceRect.getCenter().y - focusSize/2;
            x = ofClamp(x, 0, srcPixels.getWidth());
            y = ofClamp(y, 0, srcPixels.getHeight());
            srcImg.drawSubsection(0, 0, 192, 192, x, y, focusSize, focusSize);
        }
        // draw grid
        if (showGrid) grid.draw();
        else if (showGridElements) grid.drawGridElements();
        //
        if (isIdle) drawVideos();
        // Text
        if (showText) {
            ofPushStyle();
                ofSetColor(ofColor(255,50,0,220));
                int padding = 4;
                int s = 32;
                int w = s*2;
                if (textContentIndex==0) ofDrawRectangle(s*2, 0, s*2, s*2);
                if (textContentIndex==1) ofDrawRectangle(s, s*3, s*2, s*2);
                if (textContentIndex==2) ofDrawRectangle(s*4, s*4, s*2, s*2);
                ofSetColor(255);
                if (textContentIndex==0) textDisplay.at(0).drawString(wrapString(textContent.at(0), s*2-padding*2, textDisplay.at(0)), s*2+padding, 0+padding+4*2);
                if (textContentIndex==1) textDisplay.at(1).drawString(wrapString(textContent.at(1), s*2-padding*2, textDisplay.at(1)), s+padding, s*3+padding+4*2);
                if (textContentIndex==2) textDisplay.at(2).drawString(wrapString(textContent.at(2), s*2-padding*2, textDisplay.at(2)), s*4+padding, s*4+padding+4*2);
            ofPopStyle();
        }
        // draw Smiley
        ofPushStyle();
        ofPushMatrix();
            ofTranslate(161,174);
            ofSetColor(ofColor::red);
            ofDrawCircle(6, 5, 1);
            ofDrawCircle(12, 5, 1);
            ofDrawRectangle(5, 10, 8, 1);
            ofDrawBitmapString(ofToString(ft.size()), 18, 12);
        ofPopStyle();
        ofPopMatrix();
    ofPopMatrix();
    //
    guiDraw();
}


//--------------------------------------------------------------
ofPixels ofApp::getFacePart(ofPixels sourcePixels, ofPolyline partPolyline, float downScale, float zoom, float offset, bool isSquare){
    ofPoint center = partPolyline.getCentroid2D();
    // def x and y
    int x = center.x+offset;
    int y = center.y+offset;
    // def width and height
    int w,h;
    if (isSquare) {
        if ( partPolyline.getBoundingBox().width > partPolyline.getBoundingBox().height ) {
            w = partPolyline.getBoundingBox().width*1/zoom;
        } else {
            w = partPolyline.getBoundingBox().height*1/zoom;
        }
        h = w;
    } else {
        w = partPolyline.getBoundingBox().width*1/zoom;
        h = partPolyline.getBoundingBox().height*1/zoom;
    }
    // check if out of bound
    x = x-w/2;
    y = y-h/2;
    x = ofClamp(x, 0, (sourcePixels.getWidth()-w)*downScale);
    y = ofClamp(y, 0, (sourcePixels.getHeight()-h)*downScale);
    //
    sourcePixels.crop(x*downScale, y*downScale, w*downScale, h*downScale);
    return sourcePixels;
}


//--------------------------------------------------------------
void ofApp::loadVideos() {
    dir.allowExt("mov");
    dir.listDir(faceVideoPath);
    dir.sort();
    if(dir.size()>countVideos){
        videosVector.resize(countVideos);
    } else {
        videosVector.resize(dir.size());
    }
    // iterate through the files and load them into the vector
    int j = 0;
    // reverse browsing the dir
    for(int i=(int)dir.size()-1; i>=0 && j<videosVector.size(); i--){
        if ( dir.getFile(i).getSize()>100000 ) {
            videosVector[j].load(dir.getPath(i));
            videosVector[j].setLoopState(OF_LOOP_PALINDROME);
//            videosVector[j].setSpeed((int)ofGetFrameRate());
            videosVector[j].play();
            j++;
        }
    }
}


//--------------------------------------------------------------
void ofApp::drawVideos() {
    if (videosVector.size()) {
        for(int i = 0; i < videosVector.size(); i++){
            videosVector[i].draw((i%2)*96,(i/2)*96, 96, 96);
//            ofLog(OF_LOG_NOTICE, ofToString(videosVector[i].getSpeed()));
        }
    }
}

//--------------------------------------------------------------
void ofApp::updateVideos() {
    if (videosVector.size()) {
        for(int i = 0; i < videosVector.size(); i++){
            videosVector[i].update();
        }
    }
}

//--------------------------------------------------------------
void ofApp::stopVideos() {
    if (videosVector.size()) {
        for(int i = 0; i < videosVector.size(); i++){
            videosVector[i].stop();
            videosVector[i].close();
        }
    }
}


//--------------------------------------------------------------
void ofApp::randomizeSettings(){
    // ft
    float r = ofRandom(1);
    // Always the same order: face, leftEye, rightEye, mouth, nose
    if (r<.15) faceElementsQty = {(int)ofRandom(0,20), 0, 0, 0, 0};
    else if (r<.35) faceElementsQty = {0, (int)ofRandom(0,20), (int)ofRandom(0,20), 0, 0};
    else if (r<.5) faceElementsQty = {0, 0, 0, 0, (int)ofRandom(0,20)};
    else for (auto & f : faceElementsQty) f = (int)ofRandom(0,20);
    //
    for (auto & f : faceElementsOffset) f = ofRandom(0, 1);
    //
    for (int i=0; i < faceElementsZoom.size() ; i++) {
        faceElementsZoom.at(i) = (i>0) ? ofRandom(.2, .5) : ofRandom(.5, 1);
    }
    // grid
    if (ofRandom(1)>.4) {
        gridWidth = 6;
        gridHeight = 6;
        gridRes = 32;
        gridMaxSize = ofRandom(6);
    } else {
        gridWidth = 12;
        gridHeight = 12;
        gridRes = 16;
        gridMaxSize = ofRandom(12);
    }
    gridIsSquare = (ofRandom(1)>.5) ? true : false;
}


//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == 's') showText = !showText;
    if (key == '0') initLive();
    if (key == 'l') live.printAll();
    if (key == '.') {
        float v = live.getVolume();
        live.setVolume( ofClamp(v + 0.1, 0, 1) );
    }
    if (key == ',') {
        float v = live.getVolume();
        live.setVolume( ofClamp(v - 0.1, 0, 1) );
    }
}


//--------------------------------------------------------------
void ofApp::varSetup(){
    // general
    isIdle = true;
    sceneScale = 1;
    // capture
    facesFound = false;
    // video recording
    faceVideoPath = "output/face";
    playVideos = true;
    countVideos = 4;
    isRecording = false;
    // timers
    timeOut01 = 5000; // time before iddle
    timeOut02 = 2000; // time before show Capture
    timeOut03 = 1000; // time before grid
    timeOut04 = 20000; // time before starting speaking
    timeOut05 = 5000; // time to wait when finish speaking
    timer01.setup(timeOut01, false), timer02.setup(timeOut02, false), timer03.setup(timeOut03, false), timer04.setup(timeOut04, false), timer05.setup(timeOut05, false);
    // ft
    focusTime = 20; // time before focusing + recording
    smoothingRate = 1;
    enableTracking = true;
    isFocused = false;
    faceAvgWidth = 0, faceAvgHeight = 0, faceTotalFrame = 0, faceTotalWidth = 0, faceTotalHeight = 0;
    // filter
    claheClipLimit = 2;
    srcImgIsFiltered = true, srcImgIsColored = false;
    // grid
    showGrid = false;
    showGridElements = false;
    gridWidth = 6, gridHeight = 6, gridRes = 32, gridMinSize = 0, gridMaxSize = 8;
    gridIsSquare = true;
    // ft capture
    faceElementsQty.assign(5,5);
    faceElementsOffset.assign(5,0);
    faceElementsZoom.assign(5,.5);
    // live
    volumes.assign(5,0), initTimesVolumes.assign(5,0), startVolumes.assign(5,0), endVolumes.assign(5,0);
    resetLive = true;
    // text
    showText = false;
    textDisplay.resize(3);
    int i = 1;
    for (auto & t : textDisplay) {
        t.load("fonts/pixelmix.ttf", 6*i, false, false, false, 144);
        t.setLineHeight(10*i);
    }
    textFileIndex = 0, textContentIndex = 0;
    textContent.resize(3);
}


//--------------------------------------------------------------
void ofApp::guiDraw(){
    // GUI
    gui.begin();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::SliderInt("sceneScale", &sceneScale, 1, 12);
        ImGui::SliderInt("claheClipLimit", &claheClipLimit, 0, 6);
        ImGui::Checkbox("srcImgIsFiltered", &srcImgIsFiltered);
        ImGui::Checkbox("srcImgIsColored", &srcImgIsColored);
//        if (ImGui::SliderFloat("smoothingRate", &smoothingRate, 0, 6)) ft.setSmoothingRate(smoothingRate);
        if (ImGui::CollapsingHeader("Grid", false)) {
            ImGui::SliderInt("gridWidth", &gridWidth, 1, 24);
            ImGui::SliderInt("gridHeight", &gridHeight, 1, 24);
            ImGui::InputInt("gridRes", &gridRes, 8);
            ImGui::SliderInt("gridMaxSize", &gridMaxSize, 1, 12);
            ImGui::Checkbox("gridIsSquare", &gridIsSquare);
            ImGui::Checkbox("showGrid", &showGrid);
            if(ImGui::Button("Refresh Grid")) grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare);
        }
        if (ImGui::CollapsingHeader("Elements", false)) {
            ImGui::SliderInt("face Count", &faceElementsQty[0], 0, 20);
            ImGui::SliderInt("Left Eye Count", &faceElementsQty[1], 0, 20);
            ImGui::SliderInt("Right Eye Count", &faceElementsQty[2], 0, 20);
            ImGui::SliderInt("Nose Count", &faceElementsQty[3], 0, 20);
            ImGui::SliderInt("Mouth Count", &faceElementsQty[4], 0, 20);
//            ImGui::SliderFloat("Offset", &faceElementsOffset, 0, 10);
//            ImGui::SliderFloat("Zoom", &faceElementsZoom, 0.2, 6);
        }
    gui.end();
}



// LIVE
//--------------------------------------------------------------
void ofApp::initLive(){
    if (live.isLoaded()) {
        for ( int x=0; x<live.getNumTracks() ; x++ ){
            ofxAbletonLiveTrack *track = live.getTrack(x);
            track->setVolume(0);
        }
        live.setVolume(0.8);
        live.stop();
        live.play();
        live.setTempo(45);
        
    }
}


//--------------------------------------------------------------
void ofApp::refreshLive() {
    if (!live.isLoaded()) {
        return;
    }
    if (resetLive) {
        initLive();
        resetLive = false;
    }
    
    // Easing of the values
    auto duration = 2.f;
    for (int i=0; i<volumes.size(); i++) {
        if ( initTimesVolumes.at(i)!=0 ) {
            auto endTime = initTimesVolumes.at(i) + duration;
            auto now = ofGetElapsedTimef();
            volumes[i] = ofxeasing::map_clamp(now, initTimesVolumes[i], endTime, startVolumes[i], endVolumes[i], &ofxeasing::linear::easeIn);
        }
    }
    
    // set volumes
    for ( int i=0; i<live.getNumTracks() ; i++ ) live.getTrack(i)->setVolume(volumes[i]);

}


//--------------------------------------------------------------
void ofApp::exit(){
    //
    ofRemoveListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::recordingComplete);
    vidRecorder.close();
    //
    blackCam.close();
    //
    if (live.isLoaded()) live.stop();
}

//--------------------------------------------------------------
string ofApp::wrapString(string text, int width, ofTrueTypeFont textField) {
    string typeWrapped = "";
    string tempString = "";
    vector <string> words = ofSplitString(text, " ");
    for(int i=0; i<words.size(); i++) {
        string wrd = words[i];
        // if we aren't on the first word, add a space
        if (i > 0) {
            tempString += " ";
        }
        tempString += wrd;
        int stringwidth = textField.stringWidth(tempString);
        if(stringwidth >= width) {
            typeWrapped += "\n";
            tempString = wrd;		// make sure we're including the extra word on the next line
        } else if (i > 0) {
            // if we aren't on the first word, add a space
            typeWrapped += " ";
        }
        typeWrapped += wrd;
    }
    return typeWrapped;
}


//--------------------------------------------------------------
void ofApp::recordingComplete(ofxVideoRecorderOutputFileCompleteEventArgs& args){
    cout << "The recoded video file is now complete." << endl;
}


//--------------------------------------------------------------
void ofApp::keyReleased(int key){
    
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
    
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){
    
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){
    
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
    
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){
    
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
    
}
