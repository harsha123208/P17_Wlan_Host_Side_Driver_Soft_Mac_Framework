#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/ktime.h>
#define MAC_LEN 6
/* Shared by all interfaces */
struct mon_priv 
{
    u32 hardware_state;
};
/* Separate for each interface */
struct mon_vif_priv
{
    struct net_device *ndev;
    u64 tx_packets;
    u64 tx_bytes;
    u64 tx_processing_time_ns;
    u64 tx_total_bytes;

};

u8 mac_addr[MAC_LEN] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};


static struct ieee80211_hw *mon_hw;
static struct platform_device *mon_pdev;


static struct ieee80211_channel mon_channels[] = {
    { .band = NL80211_BAND_2GHZ, .center_freq = 2412,.hw_value = 1},
    { .band = NL80211_BAND_2GHZ, .center_freq = 2417, .hw_value = 2  },
    { .band = NL80211_BAND_2GHZ, .center_freq = 2422, .hw_value = 3  },
    { .band = NL80211_BAND_2GHZ, .center_freq = 2427, .hw_value = 4  },
    { .band = NL80211_BAND_2GHZ, .center_freq = 2432, .hw_value = 5  },
    { .band = NL80211_BAND_2GHZ, .center_freq = 2437, .hw_value = 6  },
};

static struct ieee80211_rate mon_rates[] = {
    { .bitrate = 10,  .hw_value = 0 },
    { .bitrate = 20,  .hw_value = 1 },
    { .bitrate = 55,  .hw_value = 2 },
    { .bitrate = 110, .hw_value = 3 },
};
static struct ieee80211_supported_band mon_band = {
    .channels = mon_channels,
    .n_channels = ARRAY_SIZE(mon_channels),

    .bitrates = mon_rates,
    .n_bitrates = ARRAY_SIZE(mon_rates),

    .ht_cap.ht_supported = false,
};

static int mon_start(struct ieee80211_hw *hw)
{
   
    struct mon_priv *priv=hw->priv;
    priv->hardware_state=1;

    pr_err("monitor: driver started\n");

    return 0;
}

static void mon_stop(struct ieee80211_hw *hw, bool suspend)
{
    struct mon_priv *priv = hw->priv;
    priv->hardware_state=0;
     
    pr_err("Hardware stopped \n");
}

static int mon_add_interface(struct ieee80211_hw *hw,struct ieee80211_vif *vif)
{
    struct wireless_dev *wdev;
    struct net_device* ndev=0;
    struct  mon_vif_priv *vpriv = (struct mon_vif_priv *)vif->drv_priv;;
    pr_info("interface created\n");

    if (vif->type == NL80211_IFTYPE_MONITOR) 
    {
        pr_info("Mode:monitor\n");
    }
    else if(vif->type == NL80211_IFTYPE_STATION)
   {
       pr_info("Mode:station mode\n");
   }


        wdev = ieee80211_vif_to_wdev(vif);

        if (!wdev) {
            pr_err("Monitor wdev is NULL\n");
            return 0;
        }

        ndev = wdev->netdev;

        if (!ndev) {
            pr_err("Monitor netdev is NULL\n");
            return 0;
        }

        vpriv->ndev=ndev;
        vpriv->tx_packets=0;
        vpriv->tx_bytes=0;
        vpriv->tx_processing_time=0;
        pr_info("Interface name = %s\n", ndev->name);
        pr_info("netdev MAC = %pM\n", ndev->dev_addr);
        pr_info("Permanent MAC = %pM\n", ndev->perm_addr);

    return 0;
}
static void mon_remove_interface(struct ieee80211_hw *hw,struct ieee80211_vif *vif)
{
    struct mon_vif_priv *vpriv =(void *)vif->drv_priv;
    pr_info("Interface removed: %s\n",vpriv->ndev ? vpriv->ndev->name : "unknown");
    vpriv->ndev = NULL; 
    vpriv->tx_packets=0;
    vpriv->tx_bytes=0;
    vpriv->tx_processing_time=0;

    pr_info("monitor: monitor interface removed\n");

}

static int mon_config(struct ieee80211_hw *hw, u32 changed)
{
struct ieee80211_conf *conf = &hw->conf;

    if (changed & IEEE80211_CONF_CHANGE_CHANNEL) 
    {
        pr_info("monitor: Channel configuration changed\n");

        if (conf->chandef.chan) 
      {
            pr_info("monitor: Frequency = %d MHz\n", conf->chandef.chan->center_freq);

            pr_info("monitor: Center Frequency = %d MHz\n",conf->chandef.center_freq1);

            pr_info("monitor: Hw value    = %d\n",conf->chandef.chan->hw_value);
        }
    } else if (changed) 
    {
        pr_info("monitor: Other configuration changed: 0x%x\n",changed);
    } else 
    {
        pr_info("monitor: No configuration changes\n");
    }

    return 0;
}

static void mon_configure_filter(struct ieee80211_hw *hw,unsigned int changed_flags,unsigned int *total_flags,u64 multicast)
{
    *total_flags = 0;

    pr_info("monitor: configure_filter called\n");
}
static void mon_rx(struct ieee80211_hw *hw,struct sk_buff *skb)
{
    struct ieee80211_rx_status *rx_status;
    unsigned int len;

    if (!skb) {
        pr_err("MON: Invalid RX packet or interface\n");

        return;
    }
    len = skb->len;
    rx_status = IEEE80211_SKB_RXCB(skb);
    memset(rx_status, 0, sizeof(*rx_status));
    rx_status->band = NL80211_BAND_2GHZ;
    rx_status->freq = 2412;
    pr_info("MON: Passing packet to mac80211 RX\n");
    ieee80211_rx(hw, skb);

}
static void mon_tx(struct ieee80211_hw *hw,struct ieee80211_tx_control *control,struct sk_buff *skb)
{
struct ieee80211_tx_info *info=NULL;
struct ieee80211_vif *vif=NULL;
struct mon_vif_priv *vpriv=NULL;
struct ieee80211_hdr *hdr=NULL;
unsigned int len=0;
u16 fc=0;
u16 type=0;
u64 start_time=0;
u64 stop_time=0;
u64 processing_time_sec=0;
/* Get TX metadata from SKB */
 start_time = ktime_get_ns();
info = IEEE80211_SKB_CB(skb);
/* Identify the originating interface */
vif = info->control.vif;

if (!vif) {
    pr_err("MON: VIF pointer is NULL\n");
    ieee80211_free_txskb(hw, skb);
    return;
}

/* Access that interface's private structure */
vpriv = (void *)vif->drv_priv;

len = skb->len;

/* Identify frame type */
if (skb->len >= 2) 
{

    hdr = (struct ieee80211_hdr *)skb->data;

    fc = le16_to_cpu(hdr->frame_control);
    type = fc & IEEE80211_FCTL_FTYPE;

    switch (type) {

    case IEEE80211_FTYPE_MGMT:
        pr_info("MON: TX Frame Type = MANAGEMENT\n");
        break;

    case IEEE80211_FTYPE_CTL:
        pr_info("MON: TX Frame Type = CONTROL\n");
        break;

    case IEEE80211_FTYPE_DATA:
        {
        pr_info("MON: TX Frame Type = DATA\n");
 
    unsigned int header_len =ieee80211_hdrlen(fc);
    unsigned int payload_len;

    pr_info("MON: TX DATA FRAME\n");

    /* Frame Control */
    pr_info("MON: Frame Control = 0x%04x\n", fc);

    /* MAC addresses */
    pr_info("MON: Destination MAC = %pM\n", hdr->addr1);
    pr_info("MON: Source MAC = %pM\n", hdr->addr2);
    pr_info("MON: BSSID = %pM\n", hdr->addr3);

    /* Sequence number */
    pr_info("MON: Sequence Number = %u\n",le16_to_cpu(hdr->seq_ctrl) >> 4);

    /* Frame length */
    pr_info("MON: Frame Length = %u bytes\n", skb->len);

    /* Payload */
    if (skb->len >= header_len) 
    {
        payload_len = skb->len - header_len;

        pr_info("MON: Payload Length = %u bytes\n", payload_len);

        pr_info("MON: Payload = %s\n",(char *)(skb->data + header_len));
    } 
    else 
    {
        pr_err("MON: Invalid DATA frame length\n");
    }

        
        break;
     }
    default:
        pr_info("MON: TX Frame Type = UNKNOWN/EXTENSION\n");
        break;
    }

} else {
    pr_err("MON: Invalid frame length\n");
}

/* Update per-interface statistics */
vpriv->tx_packets++;
vpriv->tx_bytes+=len;
/* Update corresponding network-device statistics */
if (vpriv->ndev)
    dev_sw_netstats_tx_add(vpriv->ndev,1, len);

pr_info("MON: Interface = %s\n",vpriv->ndev ? vpriv->ndev->name : "unknown");

pr_info("MON: TX packets = %lld\n",(long long)vpriv->tx_packets);

pr_info("MON: TX bytes = %lld\n",(long long)vpriv->tx_bytes);
 stop_time = ktime_get_ns();

  vpriv->tx_processing_time_ns+=stop_time-start_time;
  pr_info("time=%llu\n", processing_time_sec);
   if(vpriv->tx_processing_time_ns>1000000000ULL)
   {
      u64 result=
   }
mon_rx(hw,skb);
 return;

}


static const struct ieee80211_ops mon_ops = {
    .tx                 = mon_tx,
    .start              = mon_start,
    .stop               = mon_stop,
    .config             = mon_config,
    .add_interface      = mon_add_interface,
    .remove_interface   = mon_remove_interface,
    .configure_filter   = mon_configure_filter,
    .wake_tx_queue      = ieee80211_handle_wake_tx_queue,

    .add_chanctx        = ieee80211_emulate_add_chanctx,
    .remove_chanctx     = ieee80211_emulate_remove_chanctx,
    .change_chanctx     = ieee80211_emulate_change_chanctx,
};

static int __init mon_init(void)
{
    int ret;

    pr_info("monitor: initializing dummy WLAN driver\n");

    mon_pdev = platform_device_register_simple("monitor-dummy", -1, NULL, 0);
if (IS_ERR(mon_pdev))
{
    ret = PTR_ERR(mon_pdev);

    pr_err("Platform device registration failed: %d\n", ret);

    return ret;
}
    mon_hw = ieee80211_alloc_hw(sizeof(struct mon_priv),&mon_ops);

    if (!mon_hw) {
        pr_err("monitor: ieee80211_alloc_hw failed\n");
        ret = PTR_ERR(mon_hw);
        goto err_pdev;
    }

    SET_IEEE80211_DEV(mon_hw, &mon_pdev->dev);
    SET_IEEE80211_PERM_ADDR(mon_hw, mac_addr);

    mon_hw->wiphy->interface_modes =BIT(NL80211_IFTYPE_MONITOR);
    mon_hw->wiphy->bands[NL80211_BAND_2GHZ] =&mon_band;
    ieee80211_hw_set(mon_hw, NO_VIRTUAL_MONITOR);
    mon_hw->vif_data_size = sizeof(struct mon_vif_priv);
    ret = ieee80211_register_hw(mon_hw);
    if (ret) {
        pr_err("monitor: register_hw failed: %d\n", ret);
        goto err_free_hw;
    }

    pr_info("monitor: registered successfully\n");

    return 0;

err_free_hw:
    ieee80211_free_hw(mon_hw);
err_pdev:
    platform_device_unregister(mon_pdev);

    return ret;
}

static void __exit mon_exit(void)
{
    pr_info("monitor: unregistering driver\n");

    ieee80211_unregister_hw(mon_hw);

    ieee80211_free_hw(mon_hw);

    platform_device_unregister(mon_pdev);

    pr_info("monitor: driver removed\n");
}

module_init(mon_init);
module_exit(mon_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harsha");
MODULE_DESCRIPTION("Linux 6.13 Dummy mac80211 Monitor Driver");
