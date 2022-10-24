#include "scheduler.h"
#include <proc/process.h>

#include <arch/timer.h>
#include <utils/log.h>

#include <arch/cpu.h>
#include <utils/assert.h>

task_queue kernel_task_queue;

process_scheduler kernel_process_scheduler[NCPU];


int stride_cmp(uint64 a, uint64 b)
{	
	
	if (a==b){
		return 0;
	}
	else if (a<b){
		if(b-a>BIG_STRIDE/2){ // a overflowed
			return 1;
		}else{
			return -1;
		}
	}else{
		if(a-b>BIG_STRIDE/2){ // b overflowed
			return -1;
		}else{
			return 1;
		}
	}
}

shared_ptr<process> task_queue::pop(int core_id) {
    auto guard = make_lock_guard(lock);

    if(queue.empty()){

        // debug_core("task_queue is empty");
        return nullptr;
    }


    auto min_it = queue.begin();
    uint64 min_stride = (*min_it)->stride;
    
    for(auto it = queue.begin(); it != queue.end(); ++it) {
        if(!(*it)->ready()){
            // debugf("process %p:( name: %s ) not ready" , (*it).get(), (*it)->get_name());
            continue;
        }
        if((*it)->binding_core!=-1 && (*it)->binding_core!=core_id){
            // debugf("core %d: skip process %s, binding core %d", core_id, (*it)->get_name(), (*it)->binding_core);
            continue;
        }
        if (stride_cmp((*it)->stride, min_stride) < 0) {
            // debugf("core %d: update min stride %d", core_id, (*it)->stride);
            min_stride = (*it)->stride;
            min_it = it;
        }
    }

    if(min_it == queue.end()){
        debugf("no process ready");
        return nullptr;
    }

    auto ret = *min_it;
    queue.erase(min_it);

    return ret;
}

void task_queue::push(const shared_ptr<process>& proc) {
    auto guard = make_lock_guard(lock);
    queue.push_back(proc);
    // debug_core("push process %p: name: %s, queue_size:%d", proc.get(), proc->get_name(), queue.size());
}

bool run_process(process* p) {

    bool reschedule = false;
    // save interrupt state
    uint64 sie = r_sie();
    uint64 sstatus = r_sstatus();
    uint64 stvec = r_stvec();

    
    reschedule = p->run();

    // restore interrupt state
    w_sie(sie);
    w_sstatus(sstatus);
    w_stvec(stvec);
    
    return reschedule;
}


void process_scheduler::run()
{
    uint64 busy = 0;
    uint64 all = 0;
    uint64 timestamp1 = r_cycle();
    while(true)
    {
        
        kernel_assert(!cpu::__timer_irq_on(), "timer irq should be off");
        cpu *c = cpu::__my_cpu();
        auto process = shared_queue->pop(c->get_core_id());

        if(!process) {

            if (last_choice){
                process = last_choice;
            } else {
                warnf("no process to run");
                break;
            }
            
        }

        // debug_core("run process %d: %s, stack: %p", process->get_pid(), process->get_name(), (void*)process->get_stack_va());

        process->schedule_lock.lock();

        uint64 busy_start = r_cycle();
        process->last_start_time = timer::get_time_ms();
        uint64 pass = BIG_STRIDE / (process->priority);
        process->stride += pass;

        process->schedule_lock.unlock();
        
        c->set_process(process.get());
        bool reschedule = run_process(process.get());
        c->set_process(nullptr);

        process->schedule_lock.lock();

        // idle is not busy
        if (process != last_choice){
            busy += r_cycle() - busy_start;
        }
        
        uint64 time_delta = timer::get_time_ms() - process->last_start_time;
        process->cpu_time += time_delta;
        process->schedule_lock.unlock();

        // debug_core("process %d:%s paused, reschedule: %d", process->get_pid(), process->get_name(), reschedule);

        if (reschedule) {
            if (process != last_choice) {
                shared_queue->push(process);
            }
            
        }

        // sample cpu usage
        uint64 now = r_cycle();
        all += now - timestamp1;
        timestamp1 = now;
        // sample rate 1 Hz
        if (all > (timer::MS_TO_CYCLE(1000)))
        {
            c->sample(all, busy);
            all = 0;
            busy = 0;
        }
    }
}