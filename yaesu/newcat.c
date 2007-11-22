/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *              and the Hamlib Group (hamlib-developer at lists.sourceforge.net)
 *
 * newcat.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to any newer Yaesu radio using the
 * "new" text CAT interface.
 *
 *
 * $Id: newcat.c,v 1.1 2007-11-22 04:48:43 n0nb Exp $
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "iofunc.h"
#include "newcat.h"

/* global variables */
static char cat_term = ';';             /* Yaesu command terminator */

/*
 * future - private data
 *
 * FIXME: Does this need to be exposed to the application/frontend through
 * newcat_caps.priv? -N0NB
 */

struct newcat_priv_data {
//    unsigned char       pacing;                         /* pacing value */
//    unsigned int        read_update_delay;              /* depends on pacing value */
    vfo_t               current_vfo;                    /* active VFO from last cmd */
//    unsigned char       p_cmd[YAESU_CMD_LENGTH];        /* private copy of 1 constructed CAT cmd */
//    yaesu_cmd_set_t     pcs[FT450_NATIVE_SIZE];         /* private cmd set */
//    unsigned char       update_data[FT450_ALL_DATA_LENGTH]; /* returned data--max value, some are less */
//    unsigned char       current_mem;                    /* private memory channel number */
};


/*
 * ************************************
 *
 * Hamlib API functions
 *
 * ************************************
 */

/*
 * rig_init
 *
 */

int newcat_init(RIG *rig) {
    struct newcat_priv_data *priv;
  
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;
  
    priv = (struct newcat_priv_data *)malloc(sizeof(struct newcat_priv_data));
    if (!priv)                       /* whoops! memory shortage! */
        return -RIG_ENOMEM;
  
    /* TODO: read pacing from preferences */
//    priv->pacing = NEWCAT_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */
//    priv->read_update_delay = FT450_DEFAULT_READ_TIMEOUT; /* set update timeout to safe value */
    priv->current_vfo =  RIG_VFO_MAIN;  /* default to whatever */
    rig->state.priv = (void *)priv;
  
    return RIG_OK;
}


/*
 * rig_cleanup
 *
 * the serial port is closed by the frontend
 *
 */

int newcat_cleanup(RIG *rig) {

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;
  
    if (rig->state.priv)
        free(rig->state.priv);
    rig->state.priv = NULL;

    return RIG_OK;
}


/*
 * rig_open
 *
 * New CAT does not support pacing
 *
 */

int newcat_open(RIG *rig) {
    struct rig_state *rig_s;
    struct newcat_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    priv = (struct newcat_priv_data *)rig->state.priv;
    rig_s = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: write_delay = %i msec\n",
              __func__, rig_s->rigport.write_delay);
    rig_debug(RIG_DEBUG_TRACE, "%s: post_write_delay = %i msec\n",
              __func__, rig_s->rigport.post_write_delay);

    return RIG_OK;
}


/*
 * rig_close
 * 
 */

int newcat_close(RIG *rig) {

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    return RIG_OK;
}


/*
 * rig_set_freq
 *
 * Set frequency for a given VFO
 *
 * If vfo is set to RIG_VFO_CUR then vfo from priv_data is used.
 * If vfo differs from stored value then VFO will be set to the
 * passed vfo.
 *
 */

int newcat_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
    const struct rig_caps *caps;
    struct newcat_priv_data *priv;
    struct rig_state *rig_s;
    char cmd_str[16];           /* Up to 999.999999999 GHz ;-) */
    char c;
    int err, len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    priv = (struct newcat_priv_data *)rig->state.priv;
    caps = rig->caps;
    rig_s = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);

    /* additional debugging */
    rig_debug(RIG_DEBUG_TRACE, "%s: R2 minimum freq = %"PRIfreq" Hz\n", __func__, caps->rx_range_list2[0].start);
    rig_debug(RIG_DEBUG_TRACE, "%s: R2 maximum freq = %"PRIfreq" Hz\n", __func__, caps->rx_range_list2[0].end);

    if (freq < caps->rx_range_list1[0].start || freq > caps->rx_range_list1[0].end ||
        freq < caps->rx_range_list2[0].start || freq > caps->rx_range_list2[0].end)
        return -RIG_EINVAL;

    if (vfo == RIG_VFO_CURR) {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    } else if (vfo != priv->current_vfo) {
        /* force a VFO change if requested vfo value differs from stored value */
        err = newcat_set_vfo(rig, vfo);
        if (err != RIG_OK)
            return err;
    }

    switch (vfo) {
    case RIG_VFO_A:
        c = 'A';
        break;
    case RIG_VFO_B:
        c = 'B';
        break;
    default:
        return -RIG_ENIMPL;             /* Only VFO_A or VFO_B are valid */
    }

    /* CAT command/terminator plus variable length frequency
     * string length plus '\0' string terminator
     */
    len = snprintf(NULL, 0, "F%c%d%c", c, (int)freq, cat_term) + 1;
    if (len < 0)
        return -RIG_EINTERNAL;          /* bad news */

    /* Build the command string */
    snprintf(cmd_str, len, "F%c%d%c", c, (int)freq, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, cmd_str);

    err = write_block(&rig_s->rigport, cmd_str, strlen(cmd_str));
    if (err != RIG_OK)
        return err;

    return RIG_OK;
}


/*
 * rig_get_freq
 *
 * Return Freq for a given VFO
 *
 */

int newcat_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
    struct newcat_priv_data *priv;
    struct rig_state *rig_s;
    char cmd_str[4];            /* command + terminator */
    char ret_data[15];          /* freq string returned from rig up to 999.999999999 GHz*/
//    unsigned char *p;
//    unsigned char offset;
//    freq_t f;
//    int err, cmd_index, count;
    char c;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    if (!rig)
        return -RIG_EINVAL;
  
    priv = (struct newcat_priv_data *)rig->state.priv;
    rig_s = &rig->state;

    if (vfo == RIG_VFO_CURR) {
        err = newcat_get_vfo(rig, &priv->current_vfo);
        if (err != RIG_OK)
            return err;
        vfo = priv->current_vfo;    /* from previous get_vfo cmd */
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    switch(vfo) {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
//        cmd_index = FT890_NATIVE_VFO_DATA;
//        offset = FT890_SUMO_VFO_A_FREQ;
//        count = FT890_VFO_DATA_LENGTH;
        c = 'A';
        break;
    case RIG_VFO_B:
//        cmd_index = FT890_NATIVE_VFO_DATA;
//        offset = FT890_SUMO_VFO_B_FREQ;
//        count = FT890_VFO_DATA_LENGTH;
        c = 'B';
        break;
//    case RIG_VFO_MEM:
//    case RIG_VFO_MAIN:
//        cmd_index = FT890_NATIVE_OP_DATA;
//        offset = FT890_SUMO_DISPLAYED_FREQ;
//        count = FT890_OP_DATA_LENGTH;
//        break;
    default:
        return -RIG_EINVAL;         /* sorry, unsupported VFO */
    }

    /* Build the command string */
    snprintf(cmd_str, sizeof(cmd_str), "F%c%c", c, cat_term);
    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", cmd_str);

    /* get freq */
    err = write_block(&rig_s->rigport, cmd_str, strlen(cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&rig_s->rigport, ret_data, sizeof(ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
              __func__, err, ret_data);

    /* Check that command termination is correct */
    if (strchr(cat_term, ret_data[strlen(ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                  __func__, ret_data);
        return -RIG_EPROTO;
    }

    /* convert the read frequency string into freq_t and store in *freq */
    sscanf(ret_data+2, "%"SCNfreq, freq);

    rig_debug(RIG_DEBUG_TRACE,
              "%s: freq = %"PRIfreq" Hz for vfo 0x%02x\n", __func__, freq, vfo);

    return RIG_OK;
}


/*
 * rig_set_vfo
 *
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */

int newcat_set_vfo(RIG *rig, vfo_t vfo) {
    struct newcat_priv_data *priv;
    struct rig_state *rig_s;
    char cmd_str[5];            /* command + VFO + terminator + \0 */
    char c;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
  
    priv = (struct newcat_priv_data *)rig->state.priv;
    rig_s = &rig->state;

    if (vfo == RIG_VFO_CURR) {
        vfo = priv->current_vfo;    /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    /* FIXME: Include support for RIG_VFO_MAIN, RIG_VFO_MEM */
    switch(vfo) {
    case RIG_VFO_A:
        priv->current_vfo = vfo;    /* update active VFO */
        c = '0';
        break;
    case RIG_VFO_B:
        priv->current_vfo = vfo;
        c = '1';
        break;
//    case RIG_VFO_MEM:
        /* reset to memory channel stored by previous get_vfo
         * The recall mem channel command uses 0x01 though 0x20
         */
//        err = newcat_send_dynamic_cmd(rig, FT450_NATIVE_RECALL_MEM,
//                                     (priv->current_mem + 1), 0, 0, 0);
//        if (err != RIG_OK)
//            return err;

//        priv->current_vfo = vfo;

//        rig_debug(RIG_DEBUG_TRACE, "%s: set mem channel = 0x%02x\n",
//                  __func__, priv->current_mem);
//        return RIG_OK;
    default:
        return -RIG_ENIMPL;         /* sorry, VFO not implemented */
    }

    /* Build the command string */
    snprintf(cmd_str, sizeof(cmd_str), "VS%c%c", c, cat_term);
    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", cmd_str);

    err = write_block(&rig_s->rigport, cmd_str, strlen(cmd_str));
    if (err != RIG_OK)
        return err;

    return RIG_OK;
}


/*
 * rig_get_vfo
 *
 * get current RX vfo/mem and store requested vfo for
 * later RIG_VFO_CURR requests plus pass the tested vfo/mem
 * back to the frontend.
 *
 * TODO: determine memory status if possible
 */

int newcat_get_vfo(RIG *rig, vfo_t *vfo) {
    struct newcat_priv_data *priv;
    struct rig_state *rig_s;
    char cmd_str[] = "VS;";     /* command + terminator string */
    char ret_data[5];           /* string returned from rig */
    char c;
    int err;

//    unsigned char status_0;           /* newcat status flag 0 */
//    unsigned char stat_vfo, stat_mem; /* status tests */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;
  
    priv = (struct newcat_priv_data *)rig->state.priv;
    rig_s = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, cmd_str);

    /* Get VFO */
    err = write_block(&rig_s->rigport, cmd_str, strlen(cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&rig_s->rigport, ret_data, sizeof(ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(cat_term, ret_data[strlen(ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                  __func__, ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, VFO value = %c\n",
              __func__, err, ret_data, ret_data[2]);

    /*
     * The current VFO value is a digit ('0' or '1' ('A' or 'B' respectively))
     * embedded at ret_data[2] in the read string.
     */
    c = ret_data[2];

//    status_0 = priv->update_data[FT450_SUMO_DISPLAYED_STATUS_0];
//    stat_vfo = status_0 & SF_VFO_MASK;    /* get VFO active bits */
//    stat_mem = status_0 & SF_MEM_MASK;    /* get MEM active bits */

//    rig_debug(RIG_DEBUG_TRACE,
//              "%s: vfo status_0 = 0x%02x\n", __func__, status_0);
//    rig_debug(RIG_DEBUG_TRACE,
//              "%s: stat_vfo = 0x%02x\n", __func__, stat_vfo);
//    rig_debug(RIG_DEBUG_TRACE,
//              "%s: stat_mem = 0x%02x\n", __func__, stat_mem);

    switch (c) {
    case '0':
        *vfo = RIG_VFO_A;
        priv->current_vfo = RIG_VFO_A;
        break;
    case '1':
        *vfo = RIG_VFO_B;
        priv->current_vfo = RIG_VFO_B;
        break;
    default:
//        switch (stat_mem) {
//        case SF_MT:
//        case SF_MR:
//            *vfo = RIG_VFO_MEM;
//            priv->current_vfo = RIG_VFO_MEM;

            /*
             * Per Hamlib policy capture and store memory channel number
             * for future set_vfo command.
             */
//            err = newcat_get_update_data(rig, FT450_NATIVE_MEM_CHNL,
//                                        FT450_MEM_CHNL_LENGTH);
//            if (err != RIG_OK)
//                return err;

//            priv->current_mem = priv->update_data[FT450_SUMO_MEM_CHANNEL];

//            rig_debug(RIG_DEBUG_TRACE, "%s: stored mem channel = 0x%02x\n",
//                      __func__, priv->current_mem);
//            break;
//        default:                      /* Oops! */
//            return -RIG_EINVAL;         /* sorry, wrong current VFO */
//        }
        return -RIG_EINVAL;         /* sorry, wrong current VFO */
    }
    rig_debug(RIG_DEBUG_TRACE, "%s: set vfo = 0x%02x\n", __func__, *vfo);

    return RIG_OK;

}
