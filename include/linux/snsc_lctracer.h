/* 2013-05-10: File added and changed by Sony Corporation */
/*
 *  include/linux/snsc_lctracer.h
 *
 *  Copyright 2012 Sony Corporation
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#ifndef __LINUX_LCTRACER_H
#define __LINUX_LCTRACER_H

extern int snsc_lctracer_is_running(void);
extern void snsc_lctracer_add_trace_entry(struct task_struct *prev,
					struct task_struct *next,
					unsigned long data);
extern void snsc_lctracer_add_user_entry(const char *buffer);
extern void snsc_lctracer_start(void);
extern void snsc_lctracer_stop(void);

/* * Define IPI and local timer IRQ number.
 * Because currently 2.6.35 does not support IRQ number for them.
 * Usually IRQ number range is 0~256, so here specified IRQ number
 * is used a larger value to avoid to be same as other existing IRQ.
 */
#define SNSC_LCTRACER_IPI_IRQ	0x1000
#define SNSC_LCTRACER_LOC_IRQ	0x2000

#endif /* __LINUX_LCTRACER_H */
