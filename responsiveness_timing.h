#ifndef SCRBRD_RESPONSIVENESS_TIMING_H
#define SCRBRD_RESPONSIVENESS_TIMING_H

// Development-only maxima in microseconds. No logging or ISR in normal builds.
#ifndef SCRBRD_TIMING
#define SCRBRD_TIMING 0
#endif
#include <Arduino.h>
#if SCRBRD_TIMING
#include <atomic>
#include <cstdio>
#endif

namespace ResponsivenessTiming
{
  enum Metric { Ble, Packet, Parse, Commit, Screen, Renderer, EdgeDetection,
                PollGap, ClassificationToNavigation, NavigationToFrame, Count };
#if SCRBRD_TIMING
  inline uint32_t maxima[Count] = {};
  inline uint32_t samples[Count] = {};
  inline std::atomic<uint32_t> physicalEdgeAt{0};
  inline uint32_t lastPollAt = 0, classifiedAt = 0, navigationAt = 0;
  inline bool polled = false, navigationPending = false;
  inline void record(Metric metric, uint32_t duration)
  {
    if (duration > maxima[metric]) maxima[metric] = duration;
    ++samples[metric];
  }
  inline void poll()
  {
    const uint32_t now = micros();
    if (polled) record(PollGap, now - lastPollAt);
    lastPollAt = now; polled = true;
  }
  inline void detectedEdge() { record(EdgeDetection, micros() - physicalEdgeAt.load()); }
  inline void classified() { classifiedAt = micros(); }
  inline void navigating()
  {
    const uint32_t now = micros();
    record(ClassificationToNavigation, now - classifiedAt);
    if (!navigationPending) { navigationAt = now; navigationPending = true; }
  }
  inline void frameComplete()
  {
    if (navigationPending) record(NavigationToFrame, micros() - navigationAt);
    navigationPending = false;
  }
  inline int summary(char *buffer, size_t size)
  {
    return snprintf(buffer, size,
      "F33 us max ble=%lu pkt=%lu parse=%lu commit=%lu screen=%lu render=%lu edge=%lu poll=%lu class_nav=%lu nav_frame=%lu n_pkt=%lu n_frame=%lu\n",
      (unsigned long)maxima[Ble], (unsigned long)maxima[Packet],
      (unsigned long)maxima[Parse], (unsigned long)maxima[Commit],
      (unsigned long)maxima[Screen], (unsigned long)maxima[Renderer],
      (unsigned long)maxima[EdgeDetection], (unsigned long)maxima[PollGap],
      (unsigned long)maxima[ClassificationToNavigation], (unsigned long)maxima[NavigationToFrame],
      (unsigned long)samples[Packet], (unsigned long)samples[Screen]);
  }
#else
  inline void poll() {}
  inline void detectedEdge() {}
  inline void classified() {}
  inline void navigating() {}
  inline void frameComplete() {}
#endif
  class Scope
  {
#if SCRBRD_TIMING
    Metric metric;
    uint32_t startedAt;
    bool active = true;
#endif
  public:
    explicit Scope(Metric value)
#if SCRBRD_TIMING
      : metric(value), startedAt(micros())
#endif
    { (void)value; }
    void finish()
    {
#if SCRBRD_TIMING
      if (active) record(metric, static_cast<uint32_t>(micros()) - startedAt);
      active = false;
#endif
    }
    ~Scope() { finish(); }
  };
}
#endif
