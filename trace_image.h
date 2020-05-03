#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
//#include <initializer_list>
#include <opencv2/opencv.hpp>

#include "activity.h"
#include "memory_region.h"
#include "trace_session.h"

class TraceImage
{
public:

    class FontSettings {
    public:
        FontSettings() : face(CV_FONT_HERSHEY_TRIPLEX), scale(1.0), thickness(1) {
            cvInitFont(&font,
                       face,
                       scale, scale,
                       thickness);
        }
        FontSettings(int face, double scale, int thickness=1) : face(face), scale(scale), thickness(thickness) {
            cvInitFont(&font,
                       face,
                       scale, scale,
                       thickness);
        };

        CvFont font;
        int face;
        double scale;
        int thickness;
    };

    class moveableLabel {
    public:
        moveableLabel(cv::Point position, cv::Size size) {
            this->size = size;
            origPosition = position;
            newPosition = position;
        }

        cv::Size size;
        cv::Point origPosition, newPosition;
    };

    void saveTraceImage(std::vector<MemoryRegion> regions,
                        std::string filename,
                        std::string title = "Title",
                        bool memoryBlocks = true,
                        bool eventBlocks = true);

    TraceImage() {
        // set default fonts
        titleFont = FontSettings(CV_FONT_HERSHEY_TRIPLEX, 5.0, 5);
        memoryLabelFont = FontSettings(CV_FONT_HERSHEY_TRIPLEX, 1.5, 2);
        regionTitleFont = FontSettings(CV_FONT_HERSHEY_TRIPLEX, 3.0, 4);
        operationLabelFont = FontSettings(CV_FONT_HERSHEY_TRIPLEX, 1.3, 2);

        operationBarWidth = 100;
        imageMargin = 200;
    }

    FontSettings titleFont;
    FontSettings memoryLabelFont;
    FontSettings regionTitleFont;
    FontSettings operationLabelFont;
    int operationBarWidth;
    int imageMargin;

private:

    int findBestScaleDivisor(int size_pixels,
                             float range,
                             int targetStep_pixels,
                             std::initializer_list<float> scaleJumpsI = {2.0});

    void drawInsAxis(cv::Mat region, unsigned long instructionCount);
    void drawInstructionTick(cv::Mat target,
                             unsigned long ins,
                             cv::Point pos);

    void spreadoutLabels(std::vector<moveableLabel> *labels,
                         int min,
                         int max,
                         int border = 10);

    int getHeaderHeight();
    int getTitleHeight();
    int getOperationsWidth();

    int drawRegionTrace(cv::Mat region, MemoryRegion memRegion, bool memoryBlocks = true);
    std::vector<int> drawEventBlocks(cv::Mat region);
    void drawMemoryScale(cv::Mat region,
                         unsigned long memRange,
                         int approxPixelsPerDivision = 400);

    template <class T>
    std::string floatSigDigits(T f, int n);
};

int TraceImage::findBestScaleDivisor(int size_pixels,
                                     float range,
                                     int targetStep_pixels,
                                     std::initializer_list<float> scaleJumpsI) {

    std::vector<float> scaleJumps(scaleJumpsI);
    int jumpIdx = 0;

    // find the power of 2 step which results in a marker
    // division closest to that requested
    int testSize = 1;
    int bestSize;
    bool bestFound = false;

    float scale = size_pixels / range;

    do {
        float multiplier = scaleJumps[jumpIdx];
        jumpIdx = ++jumpIdx % scaleJumps.size();
        int testSizeStepUp = testSize * multiplier;

        int lowerDiv = testSize * scale;
        int higherDiv = testSizeStepUp * scale;

        // if the two divs straddle the target step size find the closest
        if (lowerDiv <= targetStep_pixels &&
            higherDiv > targetStep_pixels) {

            if ((lowerDiv + higherDiv) / 2 > targetStep_pixels)
                bestSize = testSize;
            else
                bestSize = testSizeStepUp;
            bestFound = true;
        }
        else
            testSize = testSizeStepUp;

    } while(!bestFound);

    return bestSize;
}

template <class T>
std::string TraceImage::floatSigDigits(T f, int n)
{
    if (f == 0) {
        return "0";
    }
    int d = (int)::ceil(::log10(f < 0 ? -f : f)); /*digits before decimal point*/
    T order = ::pow(10., n - d);
    int fRounded = (f >= 0) ? (int)(f + 0.5) : (int)(f - 0.5);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(std::max(n - d, 0)) << round(f * order) / order;
    return ss.str();
}

void TraceImage::drawInstructionTick(cv::Mat target,
                                     unsigned long ins,
                                     cv::Point pos) {

    cv::line(target,
             pos,
             pos - cv::Point(40,0),
             cv::Scalar(0, 0, 0),
             3);

    std::string tickLabel = std::to_string(ins);
    if (ins >= 1000)
        tickLabel = floatSigDigits(ins / 1000.0, 3) + " K";
    if (ins >= 1000000)
        tickLabel = floatSigDigits(ins / 1000000.0, 3) + " M";
    if (ins >= 1000000000)
        tickLabel = floatSigDigits(ins / 1000000000.0, 3) + " G";

    int baseline;
    cv::Size textSize = cv::getTextSize(tickLabel,
                                        memoryLabelFont.face,
                                        memoryLabelFont.scale,
                                        memoryLabelFont.thickness,
                                        &baseline);

    cv::putText(target,
                tickLabel,
                cv::Point(pos.x - 50 - textSize.width,
                          pos.y + textSize.height/2),
                memoryLabelFont.face,
                memoryLabelFont.scale,
                cv::Scalar(0, 0, 0),
                memoryLabelFont.thickness);
}

void TraceImage::drawInsAxis(cv::Mat region, unsigned long instructionCount) {

    int header = imageMargin + getTitleHeight() + getHeaderHeight();
    int baseline;

    int axisLength = region.rows - (2*imageMargin) - getHeaderHeight() - getTitleHeight();

    // find best tick spacing
    int insPerTick = findBestScaleDivisor(axisLength,
                                          instructionCount,
                                          300,
                                          {2.0, 2.5, 2.0});  // Jumps needed to create a 1 2 5 10 20 50 100 200 . . . sequence.

    // draw axis edge line
    cv::line(region,
             cv::Point(region.cols-1, header),
             cv::Point(region.cols-1, region.rows - imageMargin - 1),
             cv::Scalar(0, 0, 0));

    // draw tick marks and labels
    for (unsigned long ins=0; ins <= instructionCount; ins += insPerTick) {

        int pos = (ins * axisLength) / instructionCount;

        drawInstructionTick(region,
                            ins,
                            cv::Point(region.cols-1, header + pos));
    }

    // draw final tick with total instruction count
    drawInstructionTick(region,
                        instructionCount,
                        cv::Point(region.cols-1, region.rows - imageMargin));

    // draw axis label
    std::string text = "Instructions";

    cv::Size textSize = cv::getTextSize(text,
                                        regionTitleFont.face,
                                        regionTitleFont.scale,
                                        regionTitleFont.thickness,
                                        &baseline);
    cv::Mat textMat(textSize, CV_8UC3, cv::Scalar(255, 255, 255));
    cv::putText(textMat,
                text,
                cv::Point(0, textMat.rows-1),
                regionTitleFont.face,
                regionTitleFont.scale,
                cv::Scalar(0, 0, 0),
                regionTitleFont.thickness);

    //std::cout << "textMat size (" << textMat.cols << ", " << textMat.rows << "]\n";

    cv::Mat textRot(textSize.height, textSize.width, CV_8UC3);

    //std::cout << "textRot size (" << textRot.cols << ", " << textRot.rows << "]\n";

    cv::transpose(textMat, textRot);
    cv::flip(textRot, textRot, 0);
    //cv::rotate(textMat, textMat, cv::ROTATE_90_COUNTERCLOCKWISE);

    cv::Point textLoc(100, (region.rows-header)/2 - (textMat.rows/2));
    textRot.copyTo(region(cv::Rect(textLoc.x, textLoc.y,
                                   textRot.cols, textRot.rows)));
}

void TraceImage::spreadoutLabels(std::vector<moveableLabel> *labels,
                                 int min,
                                 int max,
                                 int border) {

    // use a repulsive field to iteratively move the labels until they
    // stop moving
    int totalMovement;
    do {
        // calcaulte repulsive forces
        std::vector<int> forces(labels->size(), 0);
        for (int l=0; l<labels->size()-1; ++l) {
            int dist = abs((*labels)[l+1].newPosition.y - (*labels)[l].newPosition.y);
            dist -= (*labels)[l+1].size.height / 2;
            dist -= (*labels)[l].size.height / 2;

            if (dist < border) {
                if ((*labels)[l+1].newPosition.y < (*labels)[l].newPosition.y) {
                    forces[l+1] -= 1;
                    forces[l] += 1;
                } else {
                    forces[l+1] += 1;
                    forces[l] -= 1;
                }
            }
        }

        //for (auto const& force: forces)
        //    std::cout << "force [" << force << "]\n";
        //std::cout << "pos[0] = " << (*labels)[0].newPosition.y << "\n";

        // apply forces
        totalMovement = 0;
        for (int l=0; l<labels->size(); ++l) {
            int oldy = (*labels)[l].newPosition.y;
            (*labels)[l].newPosition.y += forces[l];

            if ((*labels)[l].newPosition.y < min)
                (*labels)[l].newPosition.y = min;
            if ((*labels)[l].newPosition.y > max)
                (*labels)[l].newPosition.y = max;

            totalMovement += abs(oldy - (*labels)[l].newPosition.y);
        }

    }  while(totalMovement > 0);

}

int TraceImage::getHeaderHeight() {

    int baseline;
    cv::Size textSize = cv::getTextSize("SomeText",
                                        regionTitleFont.face,
                                        regionTitleFont.scale,
                                        regionTitleFont.thickness,
                                        &baseline);

    return textSize.height * 3;
}

int TraceImage::getTitleHeight() {

    int baseline;
    cv::Size textSize = cv::getTextSize("SomeText",
                                        titleFont.face,
                                        titleFont.scale,
                                        titleFont.thickness,
                                        &baseline);

    return textSize.height * 2.0;
}

int TraceImage::getOperationsWidth() {

    // find the width of the longest operation memory
    int widestOpLabel = 0;
    for (int a=0; a<TraceSession::activities.size(); ++a)
        if (TraceSession::activities[a].occurrences.size() > 0 &&
            TraceSession::activities[a].occurrences[0].stop > 0)
        {
            // get text box size
            int baseline;
            cv::Size textSize = cv::getTextSize(TraceSession::activities[a].name,
                                                operationLabelFont.face,
                                                operationLabelFont.scale,
                                                operationLabelFont.thickness,
                                                &baseline);
            widestOpLabel = std::max(widestOpLabel, textSize.width);
        }

    return widestOpLabel + (operationBarWidth * 2);
}

void transRectangle(cv::Mat region,
                    cv::Rect shape,
                    cv::Scalar border,
                    cv::Scalar fill,
                    int edgeWidth,
                    float alpha,
                    float outlineAlpha = 1.0) {

    cv::Size rectSize(shape.width, shape.height);
    cv::Mat canvas(rectSize, CV_8UC3, fill);
    cv::Mat targetROI = region(shape);
    cv::addWeighted(targetROI,
                               1.0-alpha,
                               canvas,
                               alpha,
                               0,
                               targetROI);

    // v crude for now.
    if (outlineAlpha > 0.0)
        cv::rectangle(region,
                      cv::Point(shape.x, shape.y),
                      cv::Point(shape.x+shape.width-1, shape.y+shape.height-1),
                      border,
                      edgeWidth);
}

int TraceImage::drawRegionTrace(cv::Mat region, MemoryRegion memRegion, bool memoryBlocks) {

    // Add title
    int headerHeight = getHeaderHeight();
    int titleHeight = getTitleHeight();
    //int scaleTop = imageMargin +  / 2;
    int baseline;
    cv::Size textSize = cv::getTextSize(memRegion.name,
                                        regionTitleFont.face,
                                        regionTitleFont.scale,
                                        regionTitleFont.thickness,
                                        &baseline);

    cv::putText(region,
                memRegion.name,
                cv::Point(region.cols/2 - textSize.width/2,
                          imageMargin + titleHeight + headerHeight/4 + textSize.height/2),
                regionTitleFont.face,
                regionTitleFont.scale,
                cv::Scalar(0, 0, 0),
                regionTitleFont.thickness);

    // draw memory scale bar
    cv::Mat scaleRegion = region(cv::Rect(0, imageMargin + titleHeight + headerHeight/2, region.cols, headerHeight/2));
    //scaleRegion = cv::Scalar(200,255,255);
    drawMemoryScale(scaleRegion,
                    memRegion.endAddr - memRegion.startAddr);

    // Add border
    cv::rectangle(region,
                  cv::Point(1, imageMargin + titleHeight + headerHeight + 1),
                  cv::Point(region.cols-1, region.rows - imageMargin - 1),
                  cv::Scalar(0, 0, 0),
                  3,
                  5);

    // Add memory trace data
    // add rows of memory operations to export_trace_image
    int traceTop = imageMargin + titleHeight + headerHeight + 1;
    if (TraceSession::showTrace) {
        cv::Vec3b storeColor(0, 0, 255);
        cv::Vec3b loadColor(255, 0, 0);
        cv::Vec3b modifyColor(0, 255, 0);
        cv::Vec3b inUseColor(235, 235, 235);
        cv::Vec3b inOutColor(170, 225, 225);
        for (int a=0; a<memRegion.resolution; ++a) {
            bool dataPresent = false;
            int lastStore = -1;
            for (int r=0; r<memRegion.trace.size(); ++r) {
                if (memRegion.trace[r][a].loadCount > 0 && memRegion.trace[r][a].storeCount > 0) {
                    if (lastStore != -1 && r-1 >= lastStore+1)
                        cv::line(region,
                                 cv::Point(a, traceTop+lastStore+1),
                                 cv::Point(a, traceTop+r-1),
                                 inUseColor);
                    lastStore = r;
                    region.at<cv::Vec3b>(traceTop+r, a) = modifyColor;
                } else if (memRegion.trace[r][a].loadCount > 0) {
                    if (lastStore != -1 && r-1 >= lastStore+1) {
                        cv::line(region,
                                 cv::Point(a, traceTop+lastStore+1),
                                 cv::Point(a, traceTop+r-1),
                                 inUseColor);
                    }
                    lastStore = r;
                    region.at<cv::Vec3b>(traceTop+r, a) = loadColor;
                } else if (memRegion.trace[r][a].storeCount > 0){
                    region.at<cv::Vec3b>(traceTop+r, a) = storeColor;
                    lastStore = r;
                }
            }
        }
    }

    if (memoryBlocks) {
        // calculate pixel position of memory areas
        for (auto &area : TraceSession::timeMemoryAreas) {
            int pixMemStart = memRegion.memAddrToPix(area.startMem);
            int pixMemEnd = memRegion.memAddrToPix(area.endMem);
            long relInsStart = area.startInstruction - TraceSession::traceStartInstruction;
            long relInsEnd = area.endInstruction - TraceSession::traceStartInstruction;
            int pixInsStart = relInsStart / TraceSession::instructionsPerRow;
            int pixInsEnd = relInsEnd / TraceSession::instructionsPerRow;

            std::cout << "Area pix [" << pixMemStart << ", " << pixInsStart << ", " << pixMemEnd << ", " << pixInsEnd << "]\n";

            if (pixMemEnd <= pixMemStart) {
                std::cout << "Error zero of negative memory size for time-memory area.";
                continue;
            }
            if (pixInsEnd <= pixInsStart) {
                std::cout << "Error zero or negative instruction size for time-memory area.";
                continue;
            }

            if (pixMemStart < 0 || pixMemEnd > region.cols) {
                std::cout << "ignoring area outside of region." << std::endl;
                continue;
            }

            transRectangle(region,
                           cv::Rect(pixMemStart, traceTop+pixInsStart, pixMemEnd - pixMemStart, pixInsEnd-pixInsStart),
                           cv::Scalar(0, 0, 0),
                           cv::Scalar(0, 255, 255),
                           3,
                           TraceSession::boxAlpha,
                           TraceSession::boxOutlineAlpha);
        }
    }

    return memRegion.resolution;
}

std::vector<int> TraceImage::drawEventBlocks(cv::Mat region) {

    std::vector<int> markerLines;

    int headerTop = imageMargin + getTitleHeight() + getHeaderHeight() + 1;

    std::vector<moveableLabel> labelPositions;

    for (int a=0; a<TraceSession::activities.size(); ++a)
        if (TraceSession::activities[a].occurrences.size() > 0 && TraceSession::activities[a].occurrences[0].stop > 0) {
            int top = headerTop + (TraceSession::activities[a].occurrences[0].start - TraceSession::traceStartInstruction) / TraceSession::instructionsPerRow;
            int bottom = headerTop + (TraceSession::activities[a].occurrences[0].stop - TraceSession::traceStartInstruction) / TraceSession::instructionsPerRow;

            // add heights to marker line array
            if (markerLines.size() == 0 || markerLines.back() != top)
                markerLines.push_back(top);
            if (markerLines.size() == 0 || markerLines.back() != bottom)
                markerLines.push_back(bottom);

            cv::rectangle(region,
                     cv::Point(0, top),
                     cv::Point(operationBarWidth, bottom),
                     cv::Scalar(0x88, 0xFF, 0x88),
                     CV_FILLED);

            cv::rectangle(region,
                          cv::Point(0, top),
                          cv::Point(operationBarWidth, bottom),
                          cv::Scalar(0, 0, 0),
                          1,
                          8);

            // get text box size
            int baseline;
            cv::Size textSize = cv::getTextSize(TraceSession::activities[a].name,
                                                operationLabelFont.face,
                                                operationLabelFont.scale,
                                                operationLabelFont.thickness,
                                                &baseline);

            cv::Point centre((operationBarWidth * 2.0) + (textSize.width / 2),
                             (top+bottom) / 2);

            labelPositions.push_back(moveableLabel(centre,
                                                   textSize));
        }

    spreadoutLabels(&labelPositions, 0, region.rows);

    int lIdx = 0;
    for (int a=0; a<TraceSession::activities.size(); ++a)
        if (TraceSession::activities[a].occurrences.size() > 0 && TraceSession::activities[a].occurrences[0].stop > 0){

            cv::Point location(labelPositions[lIdx].newPosition.x - labelPositions[lIdx].size.width / 2,
                               labelPositions[lIdx].newPosition.y + labelPositions[lIdx].size.height / 2);

            cv::putText(region,
                        TraceSession::activities[a].name,
                        location,
                        operationLabelFont.face,
                        operationLabelFont.scale,
                        cv::Scalar(0, 0, 0),
                        operationLabelFont.thickness);

            // draw a maker line from block centre to label
            cv::line(region,
                     cv::Point(operationBarWidth * 1.0, labelPositions[lIdx].origPosition.y),
                     cv::Point(operationBarWidth * 1.2, labelPositions[lIdx].origPosition.y),
                     cv::Scalar(0, 0, 0));
            cv::line(region,
                     cv::Point(operationBarWidth * 1.2, labelPositions[lIdx].origPosition.y),
                     cv::Point(operationBarWidth * 1.8, labelPositions[lIdx].newPosition.y),
                     cv::Scalar(0, 0, 0));
            cv::line(region,
                     cv::Point(operationBarWidth * 1.8, labelPositions[lIdx].newPosition.y),
                     cv::Point(operationBarWidth * 1.9, labelPositions[lIdx].newPosition.y),
                     cv::Scalar(0, 0, 0));

            ++lIdx;
        }

    return markerLines;
}

void calculateMemoryAreaInstructions() {
    for (auto &area : TraceSession::timeMemoryAreas) {
        for (auto &event : TraceSession::activities) {
            if (area.startEventAddr == event.addr &&
                event.occurrences.size() > 0)
                area.startInstruction = event.occurrences[0].start;
            if (area.endEventAddr == event.addr &&
                event.occurrences.size() > 0)
                area.endInstruction = event.occurrences[0].stop;
        }
    }

    std::cout << "Area count [" << TraceSession::timeMemoryAreas.size() << "]\n";
    for (auto &area : TraceSession::timeMemoryAreas) {
        std::cout << "area [" << area.startMem << " " << area.endMem;
        std::cout << "] ins [" << area.startInstruction << " " << area.endInstruction << "]\n";
    }
}

void TraceImage::saveTraceImage(std::vector<MemoryRegion> regions,
                                std::string filename,
                                std::string title,
                                bool memoryBlocks,
                                bool eventBlocks) {

    std::cout << "Saving memory trace plot \"" << title;
    std::cout << "\" to file \"" << filename << "\"\n";

    calculateMemoryAreaInstructions();

    int instructionAxisWidth = 500;
    int memRegionSpacing = 100;
    // calculate the size of the final image
    cv::Size imageSize(instructionAxisWidth, regions[0].trace.size() + 2);
    imageSize.width += 2 * imageMargin;
    imageSize.height += 2 * imageMargin;
    imageSize.height += getTitleHeight();
    for (auto const& region: regions) {
        imageSize.width += region.resolution + 2;
    }
    imageSize.width += memRegionSpacing * (regions.size()-1);
    imageSize.height += getHeaderHeight();
    if (eventBlocks)
        imageSize.width += getOperationsWidth();

    // create image
    cv::Mat traceImage(imageSize, CV_8UC3, cv::Scalar(255, 255, 255));

    //std::cout << "Created image with size " << imageSize << std::endl;

    // add memory regions
    int position = instructionAxisWidth + imageMargin;
    for (auto const& region: regions) {

        //std::cout << "making memory region ROI (" << position << " 0) (" << (position+region.resolution) << " " << traceImage.rows << ")\n";

        cv::Mat regionMat = traceImage(cv::Rect(position,
                                                0,
                                                region.resolution,
                                                traceImage.rows));
        //regionMat = cv::Scalar(233,255,233);
        position += memRegionSpacing + drawRegionTrace(regionMat, region);

        //std::cout << "New Position is " << position << std::endl;
    }

    //std::cout << "Final Position is " << position << std::endl;

    // add instructions axis
    cv::Mat instructionAxisRegion = traceImage(cv::Rect(imageMargin, 0, 500, traceImage.rows));
    unsigned long instructionCount = TraceSession::memoryRegions[0].trace.size() * TraceSession::instructionsPerRow;
    //std::cout << "Adding instructions axis with range " << instructionCount << std::endl;
    drawInsAxis(instructionAxisRegion, instructionCount);

    // add plot Title
    int baseline;
    cv::Size textSize = cv::getTextSize(title,
                                        titleFont.face,
                                        titleFont.scale,
                                        titleFont.thickness,
                                        &baseline);

    cv::putText(traceImage,
                title,
                cv::Point(traceImage.cols/2 - textSize.width/2,
                          imageMargin + getHeaderHeight()/2 + textSize.height/2),
                titleFont.face,
                titleFont.scale,
                cv::Scalar(0, 0, 0),
                titleFont.thickness);

    // add event labels
    bool addEventLines = false;
    if (eventBlocks) {
        cv::Mat eventsMat = traceImage(cv::Rect(position, 0,
                                                traceImage.cols-position,
                                                traceImage.rows));
        //eventsMat = cv::Scalar(255,233,233);
        std::vector<int> markerLines = drawEventBlocks(eventsMat);

        // Add horizontal lines demarking events
        if (addEventLines)
            for (auto const& height: markerLines) {
                for (int x=instructionAxisWidth+imageMargin; x<position; ++x) {
                    cv::Vec3b col = traceImage.at<cv::Vec3b>(height, x);
                    col[0] *= 0.8;
                    col[1] *= 0.8;
                    col[2] *= 0.8;
                    traceImage.at<cv::Vec3b>(height, x) = col;
                }
            }
    }

    // save image to disk
    imwrite(filename, traceImage);

    std::cout << "Complete.\n";
}

void TraceImage::drawMemoryScale(cv::Mat region,
                                 unsigned long memRange,
                                 int approxPixelsPerDivision)
{
    // find the power of 2 step which results in a marker
    // division closest to that requested
    int testSize = 1;
    int bestSize;
    bool bestFound = false;
    do {
        int lowerDiv = (testSize * region.cols) / memRange;
        int higherDiv = (2 * testSize * region.cols) / memRange;

        if (lowerDiv <= approxPixelsPerDivision &&
            higherDiv > approxPixelsPerDivision) {

            if ((lowerDiv + higherDiv) / 2 > approxPixelsPerDivision)
                bestSize = testSize;
            else
                bestSize = testSize * 2;
            bestFound = true;
        }
        else
            testSize *= 2;

    } while(!bestFound);

    int labelDivisor = 0;
    std::string labelSuffix = " B";
    if (bestSize >= 1024) {
        labelDivisor = 1024;
        labelSuffix = " KB";
    }
    if (bestSize >= 1024*1024) {
        labelDivisor = 1024*1024;
        labelSuffix = " MB";
    }

    // draw marker ticks on the image
    for (unsigned long memPos = 0; memPos <= memRange; memPos += bestSize) {
        int position = (memPos * region.cols) / memRange;
        cv::line(region,
                 cv::Point(position, region.rows/2),
                 cv::Point(position, region.rows-1),
                 cv::Scalar(0, 0, 0),
                 3);

        std::string text = std::to_string(memPos / labelDivisor) + labelSuffix;

        int baseline;
        cv::Size textSize = cv::getTextSize(text,
                                            memoryLabelFont.face,
                                            memoryLabelFont.scale,
                                            memoryLabelFont.thickness,
                                            &baseline);

        cv::putText(region,
                    text,
                    cv::Point(position - textSize.width,
                              (region.rows/2) + textSize.height/2),
                    memoryLabelFont.face,
                    memoryLabelFont.scale,
                    cv::Scalar(0, 0, 0),
                    memoryLabelFont.thickness);
    }
    cv::line(region,
             cv::Point(region.cols-1, region.rows/2),
             cv::Point(region.cols-1, region.rows-1),
             cv::Scalar(0, 0, 0),
             3);

}