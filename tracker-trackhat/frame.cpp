#include "trackhat.hpp"
#include <opencv2/imgproc.hpp>
#include "compat/math.hpp"

trackhat_preview::trackhat_preview(int w, int h)
{
    frame_bgr.create(h, w, CV_8UC3);
    frame_bgra.create(h, w, CV_8UC4);
}

void trackhat_preview::set_last_frame(const pt_frame& frame_)
{
    center = {-1, -1};
    points = frame_.as_const<trackhat_frame>()->points;
}

void trackhat_preview::draw_head_center(pt_pixel_pos_mixin::f x, pt_pixel_pos_mixin::f y)
{
    center = {x, y};
}

QImage trackhat_preview::get_bitmap()
{
    frame_bgr.setTo({0});

    draw_points();
    draw_center();

    cv::cvtColor(frame_bgr, frame_bgra, cv::COLOR_BGR2BGRA);

    return QImage((const unsigned char*) frame_bgra.data,
                  frame_bgra.cols, frame_bgra.rows,
                  (int)frame_bgra.step.p[0],
                  QImage::Format_ARGB32);
}

void trackhat_preview::draw_center()
{
    if (center == numeric_types::vec2(-1, -1))
        return;

    auto [px_, py_] = to_pixel_pos(center[0], center[1], frame_bgr.cols, frame_bgr.rows);
    int px = iround(px_), py = iround(py_);

    const f dpi = (f)frame_bgr.cols / f(320);
    constexpr int len_ = 9;
    int len = iround(len_ * dpi);

    static const cv::Scalar color(0, 255, 255);
    cv::line(frame_bgr,
             cv::Point(px - len, py),
             cv::Point(px + len, py),
             color, 1);
    cv::line(frame_bgr,
             cv::Point(px, py - len),
             cv::Point(px, py + len),
             color, 1);
}

void trackhat_preview::draw_points()
{
    for (const auto& pt : points)
    {
        if (pt.brightness == 0)
            continue;

        constexpr int sz = trackhat_camera::sensor_size;
        constexpr f scaling_factor = 5;
        const int x = pt.x * frame_bgr.cols / sz, y = pt.y * frame_bgr.rows / sz;
        const f dpi = (f)frame_bgr.cols / f(320);
        const int W = std::max(1, iround(pt.W * frame_bgr.cols * scaling_factor / sz)),
                  H = std::max(1, iround(pt.H * frame_bgr.rows * scaling_factor / sz));
        const auto point_color = progn(double c = pt.brightness; return cv::Scalar{c, c, c};);
        const auto outline_color = pt.ok
                                   ? cv::Scalar{255, 255, 0}
                                   : cv::Scalar{192, 192, 192};

        cv::ellipse(frame_bgr, cv::Point{x, y}, {W, H},
                    0, 0, 360, point_color, -1, cv::LINE_AA);
        cv::ellipse(frame_bgr, {x, y}, {iround(W + 2*dpi), iround(H + 2*dpi)},
                    0, 0, 360, outline_color, iround(dpi), cv::LINE_AA);
    }
}

void trackhat_frame::init_points(trackHat_ExtendedPoints_t points_, double min_size, double max_size)
{
    std::sort(std::begin(points_.m_point), std::end(points_.m_point),
        [](trackHat_ExtendedPoint_t p1, trackHat_ExtendedPoint_t p2) {
      return p1.m_averageBrightness > p2.m_averageBrightness;
    });

    unsigned i = 0;

    for (const trackHat_ExtendedPoint_t& pt : points_.m_point)
    {
        if (pt.m_averageBrightness == 0)
            continue;

        point p = {};

        const double radius = std::sqrt(pt.m_area) / std::sqrt(M_PI);
        if (i < 3 && radius >= min_size && radius <= max_size)
            p.ok = true;

        p.radius = radius;
        p.brightness = pt.m_averageBrightness;
        p.W = std::max(1, pt.m_boundryRigth - pt.m_boundryLeft);
        p.H = std::max(1, pt.m_boundryDown  - pt.m_boundryUp);
        p.x = trackhat_camera::sensor_size-1-pt.m_coordinateX;
        p.y = pt.m_coordinateY;

        points[i++] = p;
    }
}
