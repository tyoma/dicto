#include <agge/clipper.h>
#include <agge/filling_rules.h>
#include <agge/path.h>
#include <agge/rasterizer.h>
#include <agge/renderer.h>
#include <agge/stroke.h>
#include <agge/stroke_features.h>
#include <cstdint>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <samples/common/shell.h>
#include <stdio.h>
#include <thread>
#include <vector>

using namespace agge;
using namespace std;

#pragma warning(disable:4996)

namespace dicto
{
	class DictoApp : public application
	{
   public:
      DictoApp()
         : _filter_thread([this] { filter(); })
      {
      }

      ~DictoApp()
      {  _filter_thread.join();  }

   private:
      enum { buffer_size = 1000 };
      typedef std::int16_t sample;

      class samples_iterator : noncopyable
      {
      public:
         samples_iterator(const vector<sample> &samples, size_t max_samples, size_t max_height)
            : _samples(samples), _factor_y(0.5f * max_height / numeric_limits<std::int16_t>::max()),
               _offset_y(0.5f * max_height), _max_samples(max_samples), _x(0)
         {
         }

         unsigned vertex(real_t *x, real_t *y)
         {
            if (_x == _max_samples || _x == _samples.size())
               return path_command_stop;
            *x = 1.0f * _x;
            *y = _factor_y * _samples[_x] + _offset_y;
            return _x++ == 0 ? path_command_move_to : path_command_line_to;
         }

      private:
         const vector<sample> &_samples;
         real_t _factor_y, _offset_y;
         size_t _max_samples;
         size_t _x;
      };

   private:
		virtual void draw(platform_bitmap &surface, timings &/*timings_*/)
		{
         {
            lock_guard<mutex> lock(_buffer_mtx);

            swap(_buffer_w, _buffer_r);
         }

         if (_buffer_r.empty())
            return;

			const rect_i area = { 0, 0, static_cast<int>(surface.width()), static_cast<int>(surface.height()) };
			platform_blender_solid_color background(0, 0, 0), brush(0, 154, 255);

			_rasterizer.reset();

         fill(surface, area, background);

			_stroke.width(1.0f);
			_stroke.set_cap(caps::butt());
			_stroke.set_join(joins::bevel());

			samples_iterator i(_buffer_r, surface.width(), surface.height());
			path_generator_adapter<samples_iterator, stroke> path(i, _stroke);

			add_path(_rasterizer, path);
         _rasterizer.sort();
			_renderer(surface, 0, _rasterizer, brush, winding<>());
         _buffer_r.clear();
		}

		virtual void resize(int /*width*/, int /*height*/)
		{
		}

      void filter()
      {
         _setmode(_fileno(stdin), O_BINARY);
       	_setmode(_fileno(stdout), O_BINARY);

         char buffer[buffer_size * sizeof(sample)];

         while (!feof(stdin))
         {
            const auto samples = reinterpret_cast<const sample *>(buffer);

            auto size_read = fread(buffer, 1, buffer_size * sizeof(sample), stdin);
            fwrite(buffer, 1, size_read, stdout);

            lock_guard<mutex> lock(_buffer_mtx);
            _buffer_w.insert(_buffer_w.end(), samples, samples + size_read / sizeof(sample));
         }
      }

   private:
      thread _filter_thread;
      vector<sample> _buffer_w, _buffer_r;
      mutex _buffer_mtx;
		rasterizer< clipper<int> > _rasterizer;
		renderer _renderer;
		stroke _stroke;
	};
}

application *agge_create_application()
{
   return new dicto::DictoApp;
}
