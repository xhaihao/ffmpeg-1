/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Export subframe filter
 */

#include <stdio.h>

#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"

#include "filters.h"

typedef struct ExportSubframeContext {
    const AVClass *class;
    int w, h;
} ExportSubframeContext;

static int exportsubframe_filter_frame(AVFilterLink *inlink, AVFrame *input_frame)
{
    AVFilterContext *avctx     = inlink->dst;
    AVFilterLink *outlink      = avctx->outputs[0];
    ExportSubframeContext *ctx = avctx->priv;
    AVFrame *output_frame = NULL, *sub_frame = NULL;
    AVFrameSideData *sd = NULL;
    int ret;

    av_log(avctx, AV_LOG_DEBUG, "Filter input: %s, %ux%u (%"PRId64").\n",
           av_get_pix_fmt_name(input_frame->format),
           input_frame->width, input_frame->height, input_frame->pts);

    sd = av_frame_get_side_data(input_frame, AV_FRAME_DATA_SUB_FRAME);
    if (sd) {
        sub_frame = (AVFrame *)sd->data;
        output_frame = av_frame_alloc();
        if (!output_frame) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        if (sub_frame->width != ctx->w || sub_frame->height != ctx->h) {
            av_log(ctx, AV_LOG_ERROR, "Invalid sub frame, expect %ux%u, actual %ux%u.\n",
                   ctx->w, ctx->h, sub_frame->width, sub_frame->height);
            ret = AVERROR_INVALIDDATA;
            goto fail;
        }

        ret = av_frame_ref(output_frame, sub_frame);
        if (ret < 0)
            goto fail;

        av_frame_remove_side_data(input_frame, AV_FRAME_DATA_SUB_FRAME);

        ret = av_frame_copy_props(output_frame, input_frame);
        if (ret < 0)
            goto fail;

    } else {
         av_log(ctx, AV_LOG_ERROR, "No sub frame found.\n");
         return AVERROR_INVALIDDATA;
    }

    av_frame_free(&input_frame);

    av_log(avctx, AV_LOG_DEBUG, "Filter output: %s, %ux%u (%"PRId64").\n",
           av_get_pix_fmt_name(output_frame->format),
           output_frame->width, output_frame->height, output_frame->pts);

     return ff_filter_frame(outlink, output_frame);

fail:
    av_frame_free(&input_frame);
    av_frame_free(&output_frame);
    return ret;
}

static int config_output(AVFilterLink *outlink)
{
    AVFilterContext *avctx     = outlink->src;
    AVFilterLink *inlink       = outlink->src->inputs[0];
    ExportSubframeContext *ctx = avctx->priv;

    outlink->w = ctx->w;
    outlink->h = ctx->h;
    outlink->sample_aspect_ratio = inlink->sample_aspect_ratio;

    return 0;
}
#define OFFSET(x) offsetof(ExportSubframeContext, x)
#define FLAGS (AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_FILTERING_PARAM)
static const AVOption exportsubframe_options[] = {
    { "w", "set subframe width", OFFSET(w), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, INT_MAX, FLAGS },
    { "h", "set subframe height outputs", OFFSET(h), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, INT_MAX, FLAGS },
    { NULL }
};

AVFILTER_DEFINE_CLASS(exportsubframe);

static const AVFilterPad inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .filter_frame = &exportsubframe_filter_frame,
    },
};

static const AVFilterPad outputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .config_props = config_output,
    },
};

const AVFilter ff_vf_exportsubframe = {
    .name        = "exportsubframe",
    .description = NULL_IF_CONFIG_SMALL("Export and output subframe."),
    .priv_size   = sizeof(ExportSubframeContext),
    .priv_class  = &exportsubframe_class,
    FILTER_INPUTS(inputs),
    FILTER_OUTPUTS(outputs),
};
